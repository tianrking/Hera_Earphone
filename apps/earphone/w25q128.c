/**
 * @file w25q128.c
 * @brief W25Q128 Flash software SPI driver.
 * @note Default board wiring uses 3-wire SPI: CS=PC3, CLK=PC4, IO=PC5.
 */

#define LOG_TAG_CONST       APP
#define LOG_TAG             "[W25Q128]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#include "debug.h"

#include "w25q128.h"
#include "system/includes.h"
#include "gpio.h"
#include "asm/clock.h"
#include "asm/br28.h"
#include "asm/sfc_spi.h"

#ifndef TCFG_W25Q128_SFC_TRY_ENABLE
#define TCFG_W25Q128_SFC_TRY_ENABLE 0
#endif

static u8 w25q128_use_sfc;

#if TCFG_W25Q128_SFC_TRY_ENABLE
static struct sfc_spi_platform_data w25q128_sfc_pdata = {
    .spi_hw_index = 0,
    .sfc_data_width = SFC_DATA_WIDTH_2,
    .sfc_read_mode = SFC_RD_OUTPUT,
    .sfc_encry = 0,
    .sfc_clk_div = 16,
    .unencry_start_addr = 0,
    .unencry_size = W25Q128_CHIP_SIZE,
};

static int w25q128_id_is_valid(u32 jedec_id)
{
    jedec_id &= 0x00ffffff;
    return (jedec_id == 0x00ef4018) || (jedec_id == 0x00684018);
}

static int w25q128_sfc_probe_once(u8 spi_hw_index, enum SFC_DATA_WIDTH width,
                                  enum SFC_READ_MODE mode)
{
    u32 jedec_id;

    w25q128_sfc_pdata.spi_hw_index = spi_hw_index;
    w25q128_sfc_pdata.sfc_data_width = width;
    w25q128_sfc_pdata.sfc_read_mode = mode;

    log_info("W25Q128: SFC probe spi=%u width=%u mode=%u clk_div=%u",
             spi_hw_index, width, mode, w25q128_sfc_pdata.sfc_clk_div);

    sfc_spi_init(&w25q128_sfc_pdata);
    jedec_id = sfc_spi_read_id() & 0x00ffffff;
    log_info("W25Q128: SFC JEDEC ID = 0x%06x", jedec_id);

    if (w25q128_id_is_valid(jedec_id)) {
        w25q128_use_sfc = 1;
        log_info("W25Q128: detected by SFC/SPI%d", spi_hw_index);
        return 0;
    }

    return -1;
}

static int w25q128_sfc_probe(void)
{
    log_info("W25Q128: Trying SFC/SPI controller path first...");
    log_info("W25Q128: SPI0 Port C per SDK: CS=PC3 CLK=PC1 D0=PC2 D1=PC4 D2/WP=PC5");

    if (w25q128_sfc_probe_once(0, SFC_DATA_WIDTH_2, SFC_RD_OUTPUT) == 0) {
        return 0;
    }
    if (w25q128_sfc_probe_once(0, SFC_DATA_WIDTH_4, SFC_RD_OUTPUT) == 0) {
        return 0;
    }

    sfc_spi_close();
    log_error("W25Q128: SFC probe failed, fallback to GPIO diagnostics");
    return -1;
}
#endif

/* ==================== Software SPI Low-level Operations ==================== */

static inline void spi_delay(void)
{
    for (volatile int i = 0; i < 10; i++) {
        __asm__ volatile("nop");
    }
}

static void w25q128_release_gpio_mux(u32 pin)
{
    if ((int)pin < 0) {
        return;
    }

    gpio_disable_fun_output_port(pin);

    gpio_disable_fun_input_port(PFI_SPI1_CLK);
    gpio_disable_fun_input_port(PFI_SPI1_DA0);
    gpio_disable_fun_input_port(PFI_SPI1_DA1);
    gpio_disable_fun_input_port(PFI_SPI1_DA2);
    gpio_disable_fun_input_port(PFI_SPI1_DA3);
    gpio_disable_fun_input_port(PFI_SPI2_CLK);
    gpio_disable_fun_input_port(PFI_SPI2_DA0);
    gpio_disable_fun_input_port(PFI_SPI2_DA1);
    gpio_disable_fun_input_port(PFI_SPI2_DA2);
    gpio_disable_fun_input_port(PFI_SPI2_DA3);
    gpio_disable_fun_input_port(PFI_SD0_CMD);
    gpio_disable_fun_input_port(PFI_SD0_DA0);
    gpio_disable_fun_input_port(PFI_SD0_DA1);
    gpio_disable_fun_input_port(PFI_SD0_DA2);
    gpio_disable_fun_input_port(PFI_SD0_DA3);
    gpio_disable_fun_input_port(PFI_IIC_SCL);
    gpio_disable_fun_input_port(PFI_IIC_SDA);
}

static inline int w25q128_is_portc_pin(u32 pin)
{
    return (pin >= IO_PORTC_00) && (pin <= IO_PORTC_08);
}

static inline u32 w25q128_portc_bit(u32 pin)
{
    return BIT(pin - IO_PORTC_00);
}

static inline void w25q128_portc_output(u32 pin, u8 level)
{
    u32 bit = w25q128_portc_bit(pin);

    JL_PORTC->PU &= ~bit;
    JL_PORTC->PD &= ~bit;
    JL_PORTC->DIE |= bit;
    JL_PORTC->DIEH |= bit;
    JL_PORTC->HD |= bit;
    JL_PORTC->HD0 |= bit;
    if (level) {
        JL_PORTC->OUT |= bit;
    } else {
        JL_PORTC->OUT &= ~bit;
    }
    JL_PORTC->DIR &= ~bit;
}

static inline void w25q128_portc_input(u32 pin)
{
    u32 bit = w25q128_portc_bit(pin);

    JL_PORTC->PU &= ~bit;
    JL_PORTC->PD &= ~bit;
    JL_PORTC->DIE |= bit;
    JL_PORTC->DIEH |= bit;
    JL_PORTC->DIR |= bit;
}

static inline u8 w25q128_portc_read(u32 pin)
{
    return (JL_PORTC->IN & w25q128_portc_bit(pin)) ? 1 : 0;
}

static inline void w25q128_pin_output(u32 pin, u8 level)
{
    if ((int)pin < 0) {
        return;
    }
    w25q128_release_gpio_mux(pin);
    if (w25q128_is_portc_pin(pin)) {
        w25q128_portc_output(pin, level);
        return;
    }
    gpio_direction_output(pin, level);
    gpio_set_die(pin, 1);
    gpio_set_dieh(pin, 1);
    gpio_set_pull_up(pin, 0);
    gpio_set_pull_down(pin, 0);
    gpio_set_hd(pin, 1);
    gpio_set_hd0(pin, 1);
    gpio_write(pin, level);
}

static inline void w25q128_pin_input(u32 pin)
{
    if ((int)pin < 0) {
        return;
    }
    w25q128_release_gpio_mux(pin);
    if (w25q128_is_portc_pin(pin)) {
        w25q128_portc_input(pin);
        return;
    }
    gpio_set_direction(pin, 1);
    gpio_set_die(pin, 1);
    gpio_set_dieh(pin, 1);
    gpio_set_pull_up(pin, 0);
    gpio_set_pull_down(pin, 0);
}

static u32 w25q128_io_pin = W25Q128_IO_PIN;
static u32 w25q128_cs_pin = W25Q128_CS_PIN;
static u32 w25q128_clk_pin = W25Q128_CLK_PIN;
static u8 w25q128_power_drive_level;

static void w25q128_select_pins(u32 cs_pin, u32 clk_pin, u32 io_pin)
{
    w25q128_cs_pin = cs_pin;
    w25q128_clk_pin = clk_pin;
    w25q128_io_pin = io_pin;
}

static inline void w25q128_power_set(u8 enable)
{
    w25q128_power_drive_level = enable ? 1 : 0;

#if defined(W25Q128_POWER_SENSE_PIN)
    w25q128_pin_input(W25Q128_POWER_SENSE_PIN);
#endif
#if defined(W25Q128_POWER_PIN)
    if ((int)W25Q128_POWER_PIN >= 0) {
        w25q128_pin_output(W25Q128_POWER_PIN, enable);
        gpio_set_hd(W25Q128_POWER_PIN, enable);
        gpio_set_hd0(W25Q128_POWER_PIN, enable);
    }
#endif
}

static inline void w25q128_cs_set(u8 level)
{
    w25q128_pin_output(w25q128_cs_pin, level);
}

static inline void w25q128_clk_set(u8 level)
{
    if (w25q128_is_portc_pin(w25q128_clk_pin)) {
        if (level) {
            JL_PORTC->OUT |= w25q128_portc_bit(w25q128_clk_pin);
        } else {
            JL_PORTC->OUT &= ~w25q128_portc_bit(w25q128_clk_pin);
        }
    } else {
        gpio_write(w25q128_clk_pin, level);
    }
}

static inline void w25q128_data_output(u8 level)
{
    w25q128_pin_output(w25q128_io_pin, level);
}

static inline void w25q128_data_input(void)
{
    w25q128_pin_input(w25q128_io_pin);
}

static inline u8 w25q128_data_read(void)
{
    if (w25q128_is_portc_pin(w25q128_io_pin)) {
        return w25q128_portc_read(w25q128_io_pin);
    }
    return gpio_read(w25q128_io_pin);
}

static void w25q128_dump_pin_levels(const char *stage)
{
    log_info("W25Q128 pins [%s]: CS=%d CLK=%d IO=%d",
             stage,
             gpio_read(w25q128_cs_pin),
             gpio_read(w25q128_clk_pin),
             gpio_read(w25q128_io_pin));
#if defined(W25Q128_POWER_SENSE_PIN)
    if ((int)W25Q128_POWER_SENSE_PIN >= 0) {
        log_info("W25Q128 power [%s]: PE6 drive=%d, SDPG/PG8 sense=%d",
                 stage, w25q128_power_drive_level, gpio_read(W25Q128_POWER_SENSE_PIN));
    }
#endif
#if defined(W25Q128_POWER_PIN)
    if ((int)W25Q128_POWER_PIN >= 0) {
        log_info("W25Q128 power [%s]: PE6 readback=%d", stage, gpio_read(W25Q128_POWER_PIN));
    }
#endif
    log_info("W25Q128 PORTC [%s]: OUT=0x%08x IN=0x%08x DIR=0x%08x DIE=0x%08x PU=0x%08x PD=0x%08x",
             stage, JL_PORTC->OUT, JL_PORTC->IN, JL_PORTC->DIR,
             JL_PORTC->DIE, JL_PORTC->PU, JL_PORTC->PD);
}

static void w25q128_data_line_self_test(const char *stage)
{
    w25q128_pin_output(w25q128_cs_pin, 1);
    spi_delay();
    u8 cs_high_read = gpio_read(w25q128_cs_pin);

    w25q128_pin_output(w25q128_clk_pin, 1);
    spi_delay();
    u8 clk_high_read = gpio_read(w25q128_clk_pin);
    w25q128_pin_output(w25q128_clk_pin, 0);

    w25q128_data_output(1);
    spi_delay();
    u8 high_read = w25q128_data_read();

    w25q128_data_output(0);
    spi_delay();
    u8 low_read = w25q128_data_read();

    w25q128_data_input();
    spi_delay();
    u8 input_read = w25q128_data_read();

    log_info("W25Q128 gpio-self [%s]: CS1=%d CLK1=%d IO1=%d IO0=%d IOin=%d",
             stage, cs_high_read, clk_high_read, high_read, low_read, input_read);
}

static void soft_spi_write_byte(u8 tx_byte)
{
    w25q128_data_output(0);

    for (u8 i = 0; i < 8; i++) {
        w25q128_data_output((tx_byte & 0x80) ? 1 : 0);
        tx_byte <<= 1;

        spi_delay();
        w25q128_clk_set(1);
        spi_delay();
        w25q128_clk_set(0);
        spi_delay();
    }
}

static u8 soft_spi_read_byte(void)
{
    u8 rx_byte = 0;

    w25q128_data_input();

    for (u8 i = 0; i < 8; i++) {
        rx_byte <<= 1;

        spi_delay();
        w25q128_clk_set(1);
        spi_delay();
        if (w25q128_data_read()) {
            rx_byte |= 0x01;
        }
        w25q128_clk_set(0);
        spi_delay();
    }

    return rx_byte;
}

static u8 soft_spi_transfer_byte(u8 tx_byte)
{
    soft_spi_write_byte(tx_byte);
    return 0;
}
/* ==================== W25Q128 鍩虹鎿嶄綔 ==================== */

static int w25q128_probe_current_pins(const char *name)
{
    u8 manuf_id;
    u16 device_id;

    log_info("W25Q128: probing %s CS=0x%02x CLK=0x%02x IO=0x%02x",
             name, w25q128_cs_pin, w25q128_clk_pin, w25q128_io_pin);

    w25q128_pin_output(w25q128_clk_pin, 0);
    w25q128_data_input();
    w25q128_cs_set(1);
    w25q128_dump_pin_levels(name);
    w25q128_data_line_self_test(name);

    w25q128_release_power_down();
    os_time_dly(1);

    if (w25q128_read_jedec_id(&manuf_id, &device_id) != 0) {
        return -1;
    }

    log_info("W25Q128: JEDEC ID - Manufacturer: 0x%02x, Device: 0x%04x", manuf_id, device_id);
    if ((manuf_id == 0xEF || manuf_id == 0x68) && device_id == 0x4018) {
        log_info("W25Q128: detected on %s", name);
        return 0;
    }

    return -1;
}

/**
 * @brief 鍒濆鍖栬蒋浠?SPI 鎺ュ彛鍜?W25Q128
 */
int w25q128_init(void)
{
    log_info("W25Q128: Initializing 3-wire software SPI...");
    log_info("  schematic-map CS=0x%02x CLK=0x%02x IO=0x%02x", W25Q128_CS_PIN, W25Q128_CLK_PIN, W25Q128_IO_PIN);
#if defined(W25Q128_POWER_PIN)
    log_info("  VDD  = PE6 (0x%02x)", W25Q128_POWER_PIN);
#endif
#if defined(W25Q128_POWER_SENSE_PIN)
    log_info("  SDPG = PG8 input (0x%02x)", W25Q128_POWER_SENSE_PIN);
#endif

    w25q128_power_set(1);
    os_time_dly(20);

#if TCFG_W25Q128_SFC_TRY_ENABLE
    if (w25q128_sfc_probe() == 0) {
        return 0;
    }
#endif

    w25q128_select_pins(W25Q128_CS_PIN, W25Q128_CLK_PIN, W25Q128_IO_PIN);
    if (w25q128_probe_current_pins("schematic-map") == 0) {
        return 0;
    }

    log_error("W25Q128: Unknown or unsupported chip!");
    log_error("  Expected JEDEC: EF 40 18 or 68 40 18");
    return -1;
}
/**
 * @brief 鍙嶅垵濮嬪寲 W25Q128锛岄噴鏀惧紩鑴氳祫婧?
 */
void w25q128_deinit(void)
{
    if (w25q128_use_sfc) {
        sfc_spi_close();
        w25q128_use_sfc = 0;
    }
    w25q128_cs_set(1);
    w25q128_power_set(0);
    log_info("W25Q128: Deinitialized");
}

/**
 * @brief 璇诲彇 JEDEC ID
 */
int w25q128_read_jedec_id(u8 *manuf_id, u16 *device_id)
{
    u8 rx_buf[3];

    if (w25q128_use_sfc) {
        u32 jedec_id = sfc_spi_read_id() & 0x00ffffff;
        *manuf_id = (jedec_id >> 16) & 0xff;
        *device_id = jedec_id & 0xffff;
        log_debug("W25Q128: SFC JEDEC ID: M=0x%02x, D=0x%04x", *manuf_id, *device_id);
        return 0;
    }

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_JEDEC_ID);
    rx_buf[0] = soft_spi_read_byte();  // Manufacturer ID
    rx_buf[1] = soft_spi_read_byte();  // Device ID high byte
    rx_buf[2] = soft_spi_read_byte();  // Device ID low byte
    w25q128_cs_set(1);

    *manuf_id = rx_buf[0];
    *device_id = (rx_buf[1] << 8) | rx_buf[2];

    log_debug("W25Q128: Read JEDEC ID: M=0x%02x, D=0x%04x", *manuf_id, *device_id);

    return 0;
}

/**
 * @brief 璇诲彇鐘舵€佸瘎瀛樺櫒
 */
u8 w25q128_read_status_reg(u8 reg_num)
{
    u8 cmd;
    u8 status;

    if (w25q128_use_sfc) {
        return 0;
    }

    switch (reg_num) {
        case 1:
            cmd = W25Q128_READ_STATUS_REG1;
            break;
        case 2:
            cmd = W25Q128_READ_STATUS_REG2;
            break;
        case 3:
            cmd = W25Q128_READ_STATUS_REG3;
            break;
        default:
            return 0xFF;
    }

    w25q128_cs_set(0);
    soft_spi_transfer_byte(cmd);
    status = soft_spi_read_byte();
    w25q128_cs_set(1);

    log_debug("W25Q128: Status Reg %d = 0x%02x", reg_num, status);

    return status;
}

/**
 * @brief 绛夊緟 Flash 灏辩华
 */
int w25q128_wait_busy(u32 timeout_ms)
{
    u32 start_time = jiffies;
    u8 status;

    if (w25q128_use_sfc) {
        return 0;
    }

    while (1) {
        status = w25q128_read_status_reg(1);

        if (!(status & W25Q128_SR_BUSY_MASK)) {
            // BUSY 浣嶄负 0锛岃〃绀哄氨缁?
            return 0;
        }

        // 瓒呮椂妫€鏌?
        if (timeout_ms > 0 && (jiffies - start_time) >= timeout_ms) {
            log_error("W25Q128: Wait busy timeout!");
            return -1;
        }

        // 鐭殏寤舵椂
        os_time_dly(1);  // 1ms
    }
}

/**
 * @brief 鍐欎娇鑳?
 */
void w25q128_write_enable(void)
{
    if (w25q128_use_sfc) {
        return;
    }

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_WRITE_ENABLE);
    w25q128_cs_set(1);

    log_debug("W25Q128: Write enable");
}

/**
 * @brief 鍐欑姝?
 */
void w25q128_write_disable(void)
{
    if (w25q128_use_sfc) {
        return;
    }

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_WRITE_DISABLE);
    w25q128_cs_set(1);

    log_debug("W25Q128: Write disable");
}

/**
 * @brief 璇诲彇 Flash 鏁版嵁
 */
int w25q128_read_data(u32 addr, u8 *buf, u32 len)
{
    int ret;

    if (buf == NULL || len == 0) {
        return -1;
    }

    if (w25q128_use_sfc) {
        ret = sfc_spi_read(addr, buf, len);
        if (ret != len) {
            log_error("W25Q128: SFC read failed addr=0x%06x len=%u ret=%d", addr, len, ret);
            return -1;
        }
        return 0;
    }

    log_debug("W25Q128: Read %u bytes from 0x%06x", len, addr);

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_READ_DATA);
    soft_spi_transfer_byte((addr >> 16) & 0xFF);  // 鍦板潃楂樺瓧鑺?
    soft_spi_transfer_byte((addr >> 8) & 0xFF);   // 鍦板潃涓瓧鑺?
    soft_spi_transfer_byte(addr & 0xFF);          // 鍦板潃浣庡瓧鑺?

    for (u32 i = 0; i < len; i++) {
        buf[i] = soft_spi_read_byte();
    }

    w25q128_cs_set(1);

    return 0;
}

/**
 * @brief 椤电紪绋?
 */
int w25q128_page_program(u32 addr, const u8 *buf, u32 len)
{
    int ret;

    if (buf == NULL || len == 0 || len > W25Q128_PAGE_SIZE) {
        log_error("W25Q128: Invalid page program parameters!");
        return -1;
    }

    // 妫€鏌ラ〉杈圭晫
    u32 page_offset = addr % W25Q128_PAGE_SIZE;
    if (page_offset + len > W25Q128_PAGE_SIZE) {
        log_error("W25Q128: Page program crosses page boundary!");
        return -2;
    }

    if (w25q128_use_sfc) {
        ret = sfc_spi_write_pages(addr, (void *)buf, len);
        if (ret != len) {
            log_error("W25Q128: SFC page program failed addr=0x%06x len=%u ret=%d", addr, len, ret);
            return -1;
        }
        return 0;
    }

    log_debug("W25Q128: Page program %u bytes to 0x%06x", len, addr);

    // 鍐欎娇鑳?
    w25q128_write_enable();

    // 绛夊緟鍐欎娇鑳界敓鏁?
    u8 status = w25q128_read_status_reg(1);
    if (!(status & W25Q128_SR_WEL_MASK)) {
        log_error("W25Q128: Write enable failed!");
        return -3;
    }

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_PAGE_PROGRAM);
    soft_spi_transfer_byte((addr >> 16) & 0xFF);
    soft_spi_transfer_byte((addr >> 8) & 0xFF);
    soft_spi_transfer_byte(addr & 0xFF);

    for (u32 i = 0; i < len; i++) {
        soft_spi_transfer_byte(buf[i]);
    }

    w25q128_cs_set(1);

    // 绛夊緟鍐欐搷浣滃畬鎴?
    if (w25q128_wait_busy(100) != 0) {
        log_error("W25Q128: Page program timeout!");
        return -4;
    }

    return 0;
}

/**
 * @brief 鍐欏叆鏁版嵁 (鑷姩澶勭悊椤靛榻愬拰鍒嗛〉鍐欏叆)
 */
int w25q128_write_data(u32 addr, const u8 *buf, u32 len)
{
    if (buf == NULL || len == 0) {
        return -1;
    }

    log_info("W25Q128: Write %u bytes to 0x%06x", len, addr);

    u32 offset = 0;
    u32 current_addr = addr;

    while (offset < len) {
        // 璁＄畻褰撳墠椤电殑鍓╀綑绌洪棿
        u32 page_offset = current_addr % W25Q128_PAGE_SIZE;
        u32 page_space = W25Q128_PAGE_SIZE - page_offset;

        // 璁＄畻鏈鍐欏叆闀垮害
        u32 write_len = (len - offset) < page_space ? (len - offset) : page_space;

        // 鎵ц椤电紪绋?
        int ret = w25q128_page_program(current_addr, buf + offset, write_len);
        if (ret != 0) {
            log_error("W25Q128: Page program failed at offset %u!", offset);
            return ret;
        }

        offset += write_len;
        current_addr += write_len;
    }

    log_info("W25Q128: Write %u bytes completed", len);

    return 0;
}

/**
 * @brief 鎵囧尯鎿﹂櫎 (4KB)
 */
int w25q128_sector_erase(u32 addr)
{
    if (addr % W25Q128_SECTOR_SIZE != 0) {
        log_error("W25Q128: Address not 4KB aligned!");
        return -1;
    }

    if (w25q128_use_sfc) {
        return sfc_spi_eraser(IOCTL_ERASE_SECTOR, addr);
    }

    log_info("W25Q128: Sector erase at 0x%06x", addr);

    w25q128_write_enable();

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_SECTOR_ERASE_4KB);
    soft_spi_transfer_byte((addr >> 16) & 0xFF);
    soft_spi_transfer_byte((addr >> 8) & 0xFF);
    soft_spi_transfer_byte(addr & 0xFF);
    w25q128_cs_set(1);

    // 绛夊緟鎿﹂櫎瀹屾垚 (鍏稿瀷鍊? 45ms, 鏈€澶у€? 400ms)
    if (w25q128_wait_busy(500) != 0) {
        log_error("W25Q128: Sector erase timeout!");
        return -2;
    }

    return 0;
}

/**
 * @brief 鍧楁摝闄?(32KB)
 */
int w25q128_block_erase_32kb(u32 addr)
{
    if (addr % W25Q128_BLOCK_SIZE_32KB != 0) {
        log_error("W25Q128: Address not 32KB aligned!");
        return -1;
    }

    if (w25q128_use_sfc) {
        return sfc_spi_eraser(IOCTL_ERASE_BLOCK, addr);
    }

    log_info("W25Q128: Block erase (32KB) at 0x%06x", addr);

    w25q128_write_enable();

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_BLOCK_ERASE_32KB);
    soft_spi_transfer_byte((addr >> 16) & 0xFF);
    soft_spi_transfer_byte((addr >> 8) & 0xFF);
    soft_spi_transfer_byte(addr & 0xFF);
    w25q128_cs_set(1);

    // 绛夊緟鎿﹂櫎瀹屾垚 (鍏稿瀷鍊? 120ms, 鏈€澶у€? 1600ms)
    if (w25q128_wait_busy(2000) != 0) {
        log_error("W25Q128: Block erase timeout!");
        return -2;
    }

    return 0;
}

/**
 * @brief 鍧楁摝闄?(64KB)
 */
int w25q128_block_erase_64kb(u32 addr)
{
    if (addr % W25Q128_BLOCK_SIZE_64KB != 0) {
        log_error("W25Q128: Address not 64KB aligned!");
        return -1;
    }

    if (w25q128_use_sfc) {
        return sfc_spi_eraser(IOCTL_ERASE_BLOCK, addr);
    }

    log_info("W25Q128: Block erase (64KB) at 0x%06x", addr);

    w25q128_write_enable();

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_BLOCK_ERASE_64KB);
    soft_spi_transfer_byte((addr >> 16) & 0xFF);
    soft_spi_transfer_byte((addr >> 8) & 0xFF);
    soft_spi_transfer_byte(addr & 0xFF);
    w25q128_cs_set(1);

    // 绛夊緟鎿﹂櫎瀹屾垚 (鍏稿瀷鍊? 150ms, 鏈€澶у€? 2000ms)
    if (w25q128_wait_busy(2500) != 0) {
        log_error("W25Q128: Block erase timeout!");
        return -2;
    }

    return 0;
}

/**
 * @brief 鑺墖鎿﹂櫎
 */
int w25q128_chip_erase(void)
{
    log_info("W25Q128: Chip erase started...");

    if (w25q128_use_sfc) {
        return sfc_spi_eraser(IOCTL_ERASE_CHIP, 0);
    }

    w25q128_write_enable();

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_CHIP_ERASE);
    w25q128_cs_set(1);

    // 绛夊緟鎿﹂櫎瀹屾垚 (鍏稿瀷鍊? 5s, 鏈€澶у€? 32s)
    if (w25q128_wait_busy(35000) != 0) {
        log_error("W25Q128: Chip erase timeout!");
        return -1;
    }

    log_info("W25Q128: Chip erase completed!");
    return 0;
}

/**
 * @brief 杩涘叆鎺夌數妯″紡
 */
void w25q128_power_down(void)
{
    if (w25q128_use_sfc) {
        return;
    }

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_POWER_DOWN);
    w25q128_cs_set(1);

    log_info("W25Q128: Power down mode");

    // 绛夊緟 tDP (鏈€澶?3us)
    spi_delay();
    spi_delay();
    spi_delay();
}

/**
 * @brief 閫€鍑烘帀鐢垫ā寮?
 */
void w25q128_release_power_down(void)
{
    if (w25q128_use_sfc) {
        return;
    }

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_RELEASE_POWER_DOWN);
    w25q128_cs_set(1);

    log_info("W25Q128: Release power down");

    // 绛夊緟 tRES1 (鏈€灏?20us)
    for (int i = 0; i < 20; i++) {
        spi_delay();
    }
}

/**
 * @brief W25Q128 璇诲啓娴嬭瘯鍑芥暟
 */
int w25q128_test(void)
{
    int ret;
    u8 write_buf[512];  // 澧炲姞鍒?512 瀛楄妭浠ユ敮鎸佽法椤垫祴璇?
    u8 read_buf[512];
    u8 test_passed = 1;

    printf("\n========================================\n");
    printf("   W25Q128 Flash Read/Write Test\n");
    printf("========================================\n");

    // 0. 绠€鍗曟祴璇曪細鍐欏叆 0x11223344 骞惰鍥?
    printf("\n[0] Quick Test: Write 0x11223344 and Read Back...\n");
    u32 test_addr = 0x001000;  // 浣跨敤 0x1000 鍦板潃閬垮厤瑕嗙洊閲嶈鏁版嵁

    // 鎿﹂櫎璇ユ墖鍖?
    printf("    Erasing sector at 0x%06x...\n", test_addr);
    ret = w25q128_sector_erase(test_addr);
    if (ret != 0) {
        printf("    [FAILED] Sector erase failed!\n");
    } else {
        // 鍑嗗 4 瀛楄妭鏁版嵁: 0x11 0x22 0x33 0x44
        write_buf[0] = 0x11;
        write_buf[1] = 0x22;
        write_buf[2] = 0x33;
        write_buf[3] = 0x44;

        printf("    Writing 4 bytes: 0x11 0x22 0x33 0x44 to 0x%06x...\n", test_addr);
        ret = w25q128_page_program(test_addr, write_buf, 4);
        if (ret == 0) {
            printf("    Write OK!\n");

            // 璇诲洖鏁版嵁
            printf("    Reading back 4 bytes from 0x%06x...\n", test_addr);
            ret = w25q128_read_data(test_addr, read_buf, 4);
            if (ret == 0) {
                printf("    Read OK!\n");
                printf("    Data read: 0x%02x 0x%02x 0x%02x 0x%02x\n",
                       read_buf[0], read_buf[1], read_buf[2], read_buf[3]);

                // 楠岃瘉鏁版嵁
                if (read_buf[0] == 0x11 && read_buf[1] == 0x22 &&
                    read_buf[2] == 0x33 && read_buf[3] == 0x44) {
                    printf("    [PASSED] Data matches! 0x11223344 written and verified!\n");
                } else {
                    printf("    [FAILED] Data mismatch!\n");
                    printf("    Expected: 0x11 0x22 0x33 0x44\n");
                    printf("    Got:      0x%02x 0x%02x 0x%02x 0x%02x\n",
                           read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
                    test_passed = 0;
                }
            } else {
                printf("    [FAILED] Read failed!\n");
                test_passed = 0;
            }
        } else {
            printf("    [FAILED] Write failed!\n");
            test_passed = 0;
        }
    }

    // 1. 璇诲彇 JEDEC ID
    printf("\n[1] Reading JEDEC ID...\n");
    u8 manuf_id;
    u16 device_id;
    ret = w25q128_read_jedec_id(&manuf_id, &device_id);
    if (ret == 0) {
        printf("    Manufacturer ID: 0x%02x %s\n", manuf_id,
               (manuf_id == 0xEF) ? "(Winbond)" : "");
        printf("    Device ID: 0x%04x\n", device_id);
        if (device_id == 0x17) {
            printf("    Chip: W25Q128 (16MB) detected!\n");
        }
    } else {
        printf("    [FAILED] Cannot read JEDEC ID!\n");
        return -1;
    }

    // 2. 璇诲彇鐘舵€佸瘎瀛樺櫒
    printf("\n[2] Reading Status Registers...\n");
    u8 sr1 = w25q128_read_status_reg(1);
    u8 sr2 = w25q128_read_status_reg(2);
    u8 sr3 = w25q128_read_status_reg(3);
    printf("    SR1: 0x%02x, SR2: 0x%02x, SR3: 0x%02x\n", sr1, sr2, sr3);
    printf("    Busy: %d, WEL: %d\n", (sr1 & 0x01) ? 1 : 0, (sr1 & 0x02) ? 1 : 0);

    // 3. 鎵囧尯鎿﹂櫎娴嬭瘯
    printf("\n[3] Erasing Test Sector (4KB at 0x000000)...\n");
    ret = w25q128_sector_erase(0x000000);
    if (ret == 0) {
        printf("    Sector erase OK!\n");
    } else {
        printf("    [FAILED] Sector erase failed!\n");
        test_passed = 0;
    }

    // 4. 鍑嗗娴嬭瘯鏁版嵁
    printf("\n[4] Preparing Test Data...\n");
    for (int i = 0; i < 256; i++) {
        write_buf[i] = (u8)(i * 0x11);  // 0x11, 0x22, 0x33, ..., 0xff (閲嶅)
    }
    printf("    Test pattern: 0x11, 0x22, 0x33, ..., 0xff\n");

    // 5. 鍐欏叆娴嬭瘯
    printf("\n[5] Writing 256 bytes to address 0x000000...\n");
    ret = w25q128_page_program(0x000000, write_buf, 256);
    if (ret == 0) {
        printf("    Write OK!\n");
    } else {
        printf("    [FAILED] Write failed!\n");
        test_passed = 0;
    }

    // 6. 璇诲彇楠岃瘉
    printf("\n[6] Reading back 256 bytes from address 0x000000...\n");
    ret = w25q128_read_data(0x000000, read_buf, 256);
    if (ret == 0) {
        printf("    Read OK!\n");
    } else {
        printf("    [FAILED] Read failed!\n");
        test_passed = 0;
    }

    // 7. 鏁版嵁姣旇緝
    printf("\n[7] Verifying Data...\n");
    int error_count = 0;
    for (int i = 0; i < 256; i++) {
        if (read_buf[i] != write_buf[i]) {
            if (error_count < 10) {  // 鍙樉绀哄墠10涓敊璇?
                printf("    Mismatch at offset %d: expected 0x%02x, got 0x%02x\n",
                       i, write_buf[i], read_buf[i]);
            }
            error_count++;
        }
    }

    if (error_count == 0) {
        printf("    All data verified OK! (256 bytes)\n");
    } else {
        printf("    [FAILED] %d byte(s) mismatch!\n", error_count);
        test_passed = 0;
    }

    // 8. 鏄剧ず閮ㄥ垎鏁版嵁
    printf("\n[8] Sample Data Display:\n");
    printf("    Written: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", write_buf[i]);
    }
    printf("\n    Read:    ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", read_buf[i]);
    }
    printf("\n");

    // 9. 璺ㄩ〉鍐欏叆娴嬭瘯
    printf("\n[9] Cross-Page Write Test (512 bytes)...\n");
    for (int i = 0; i < 512; i++) {
        write_buf[i] = (u8)(i + 100);  // 100, 101, ..., 611
    }
    ret = w25q128_write_data(0x000100, write_buf, 512);
    if (ret == 0) {
        printf("    Cross-page write OK!\n");
    } else {
        printf("    [FAILED] Cross-page write failed!\n");
        test_passed = 0;
    }

    // 10. 璺ㄩ〉璇诲彇楠岃瘉
    printf("\n[10] Cross-Page Read & Verify Test...\n");
    ret = w25q128_read_data(0x000100, read_buf, 512);
    if (ret == 0) {
        error_count = 0;
        for (int i = 0; i < 512; i++) {
            if (read_buf[i] != write_buf[i]) {
                error_count++;
            }
        }
        if (error_count == 0) {
            printf("    Cross-page read & verify OK! (512 bytes)\n");
        } else {
            printf("    [FAILED] Cross-page verify: %d mismatch(es)!\n", error_count);
            test_passed = 0;
        }
    } else {
        printf("    [FAILED] Cross-page read failed!\n");
        test_passed = 0;
    }

    // 娴嬭瘯缁撴灉
    printf("\n========================================\n");
    if (test_passed) {
        printf("   TEST RESULT: PASSED!\n");
    } else {
        printf("   TEST RESULT: FAILED!\n");
    }
    printf("========================================\n\n");

    return test_passed ? 0 : -1;
}






