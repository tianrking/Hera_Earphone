/**
 * @file w25q128.c
 * @brief W25Q128 Flash 芯片软件 SPI 驱动实现 - 三线SPI模式
 * @note 引脚定义: IO=PG0 (MOSI/MISO共用), CS=PB2, CLK=PB1
 * @note SPI 频率: 1MHz (周期约 1us)
 * @note 三线模式: MOSI和MISO共用一个引脚，通过切换输入/输出模式实现
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

/* ==================== 软件SPI低层操作 ==================== */

// SPI 延时 - 用于控制时钟频率
// 1MHz = 1us 周期，半周期约 500ns
// 简单延时函数，实际频率可能需要根据系统时钟调整
static inline void spi_delay(void)
{
    // 系统时钟通常是 48MHz 或更高
    // 1MHz SPI 时钟 -> 每半周期约 24 个指令周期 (假设48MHz)
    // 使用简单的循环延时
    for (volatile int i = 0; i < 10; i++) {
        __asm__ volatile("nop");
    }
}

/**
 * @brief 设置 CS 电平
 */
static inline void w25q128_cs_set(u8 level)
{
    gpio_set_direction(W25Q128_CS_PIN, 0);  // 输出模式
    gpio_set_die(W25Q128_CS_PIN, 1);
    gpio_set_pull_up(W25Q128_CS_PIN, 0);
    gpio_set_pull_down(W25Q128_CS_PIN, 0);
    gpio_write(W25Q128_CS_PIN, level);
}

/**
 * @brief 设置 CLK 电平
 */
static inline void w25q128_clk_set(u8 level)
{
    gpio_write(W25Q128_CLK_PIN, level);
}

/**
 * @brief 设置 IO 引脚为输出模式并设置电平
 */
static inline void w25q128_io_set_output(u8 level)
{
    gpio_set_direction(W25Q128_IO_PIN, 0);  // 输出模式
    gpio_set_die(W25Q128_IO_PIN, 1);
    gpio_set_pull_up(W25Q128_IO_PIN, 0);
    gpio_set_pull_down(W25Q128_IO_PIN, 0);
    gpio_write(W25Q128_IO_PIN, level);
}

/**
 * @brief 设置 IO 引脚为输入模式并读取电平
 */
static inline u8 w25q128_io_read(void)
{
    gpio_set_direction(W25Q128_IO_PIN, 1);  // 输入模式
    gpio_set_die(W25Q128_IO_PIN, 1);
    gpio_set_pull_up(W25Q128_IO_PIN, 0);
    gpio_set_pull_down(W25Q128_IO_PIN, 0);
    return gpio_read(W25Q128_IO_PIN);
}

/**
 * @brief 软件SPI发送/接收一个字节 - 三线模式
 * @param tx_byte 要发送的字节
 * @return 接收到的字节
 */
static u8 soft_spi_transfer_byte(u8 tx_byte)
{
    u8 rx_byte = 0;

    for (u8 i = 0; i < 8; i++) {
        // SPI Mode 0: CPOL=0, CPHA=0
        // 时钟空闲为低，在上升沿采样数据

        // 设置 IO 为输出并写入数据位
        if (tx_byte & 0x80) {
            w25q128_io_set_output(1);
        } else {
            w25q128_io_set_output(0);
        }
        tx_byte <<= 1;

        spi_delay();

        // 时钟上升沿
        w25q128_clk_set(1);

        spi_delay();

        // 切换 IO 为输入并读取数据位
        rx_byte <<= 1;
        if (w25q128_io_read()) {
            rx_byte |= 0x01;
        }

        // 时钟下降沿
        w25q128_clk_set(0);

        spi_delay();
    }

    return rx_byte;
}

/* ==================== W25Q128 基础操作 ==================== */

/**
 * @brief 初始化软件 SPI 接口和 W25Q128 - 三线模式
 */
int w25q128_init(void)
{
    // log_info("W25Q128: Initializing 3-wire SPI...");
    // log_info("  IO   = PG0 (0x%02x) - MOSI/MISO shared", W25Q128_IO_PIN);
    // log_info("  CS   = PB2 (0x%02x)", W25Q128_CS_PIN);
    // log_info("  CLK  = PB1 (0x%02x)", W25Q128_CLK_PIN);

    log_info("W25Q128: Initializing 3-wire SPI...");
    log_info("  IO   = IO_PORTC_05 (0x%02x) - MOSI/MISO shared", W25Q128_IO_PIN);
    log_info("  CS   = IO_PORTC_03 (0x%02x)", W25Q128_CS_PIN);
    log_info("  CLK  = IO_PORTC_04 (0x%02x)", W25Q128_CLK_PIN);

    // 配置 IO 引脚 - 默认为输出模式
    gpio_set_direction(W25Q128_IO_PIN, 0);
    gpio_set_die(W25Q128_IO_PIN, 1);
    gpio_set_pull_up(W25Q128_IO_PIN, 0);
    gpio_set_pull_down(W25Q128_IO_PIN, 0);
    gpio_write(W25Q128_IO_PIN, 0);

    // 配置 CS - 输出
    gpio_set_direction(W25Q128_CS_PIN, 0);
    gpio_set_die(W25Q128_CS_PIN, 1);
    gpio_set_pull_up(W25Q128_CS_PIN, 0);
    gpio_set_pull_down(W25Q128_CS_PIN, 0);

    // 配置 CLK - 输出
    gpio_set_direction(W25Q128_CLK_PIN, 0);
    gpio_set_die(W25Q128_CLK_PIN, 1);
    gpio_set_pull_up(W25Q128_CLK_PIN, 0);
    gpio_set_pull_down(W25Q128_CLK_PIN, 0);
    gpio_write(W25Q128_CLK_PIN, 0);

    // CS 默认为高电平 (未选中)
    w25q128_cs_set(1);

    // 读取 JEDEC ID 验证芯片
    u8 manuf_id;
    u16 device_id;
    if (w25q128_read_jedec_id(&manuf_id, &device_id) == 0) {
        log_info("W25Q128: JEDEC ID - Manufacturer: 0x%02x, Device: 0x%04x", manuf_id, device_id);
        // 支持 Winbond (0xEF) 和 GigaDevice (0x68) 的 128Mbit Flash
        if (manuf_id == 0xEF) {  // Winbond
            log_info("W25Q128: Winbond W25Q128 detected!");
            return 0;
        } else if (manuf_id == 0x68) {  // GigaDevice
            if (device_id == 0x4018) {
                log_info("W25Q128: GigaDevice GD25Q128 detected!");
                return 0;
            }
        }
    }

    log_error("W25Q128: Unknown or unsupported chip!");
    log_error("  Expected: Winbond(0xEF) or GigaDevice(0x68)");
    return -1;
}

/**
 * @brief 反初始化 W25Q128，释放引脚资源
 */
void w25q128_deinit(void)
{
    w25q128_cs_set(1);
    log_info("W25Q128: Deinitialized");
}

/**
 * @brief 读取 JEDEC ID
 */
int w25q128_read_jedec_id(u8 *manuf_id, u16 *device_id)
{
    u8 rx_buf[3];

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_JEDEC_ID);
    rx_buf[0] = soft_spi_transfer_byte(0xFF);  // Manufacturer ID
    rx_buf[1] = soft_spi_transfer_byte(0xFF);  // Device ID high byte
    rx_buf[2] = soft_spi_transfer_byte(0xFF);  // Device ID low byte
    w25q128_cs_set(1);

    *manuf_id = rx_buf[0];
    *device_id = (rx_buf[1] << 8) | rx_buf[2];

    log_debug("W25Q128: Read JEDEC ID: M=0x%02x, D=0x%04x", *manuf_id, *device_id);

    return 0;
}

/**
 * @brief 读取状态寄存器
 */
u8 w25q128_read_status_reg(u8 reg_num)
{
    u8 cmd;
    u8 status;

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
    status = soft_spi_transfer_byte(0xFF);
    w25q128_cs_set(1);

    log_debug("W25Q128: Status Reg %d = 0x%02x", reg_num, status);

    return status;
}

/**
 * @brief 等待 Flash 就绪
 */
int w25q128_wait_busy(u32 timeout_ms)
{
    u32 start_time = jiffies;
    u8 status;

    while (1) {
        status = w25q128_read_status_reg(1);

        if (!(status & W25Q128_SR_BUSY_MASK)) {
            // BUSY 位为 0，表示就绪
            return 0;
        }

        // 超时检查
        if (timeout_ms > 0 && (jiffies - start_time) >= timeout_ms) {
            log_error("W25Q128: Wait busy timeout!");
            return -1;
        }

        // 短暂延时
        os_time_dly(1);  // 1ms
    }
}

/**
 * @brief 写使能
 */
void w25q128_write_enable(void)
{
    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_WRITE_ENABLE);
    w25q128_cs_set(1);

    log_debug("W25Q128: Write enable");
}

/**
 * @brief 写禁止
 */
void w25q128_write_disable(void)
{
    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_WRITE_DISABLE);
    w25q128_cs_set(1);

    log_debug("W25Q128: Write disable");
}

/**
 * @brief 读取 Flash 数据
 */
int w25q128_read_data(u32 addr, u8 *buf, u32 len)
{
    if (buf == NULL || len == 0) {
        return -1;
    }

    log_debug("W25Q128: Read %u bytes from 0x%06x", len, addr);

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_READ_DATA);
    soft_spi_transfer_byte((addr >> 16) & 0xFF);  // 地址高字节
    soft_spi_transfer_byte((addr >> 8) & 0xFF);   // 地址中字节
    soft_spi_transfer_byte(addr & 0xFF);          // 地址低字节

    for (u32 i = 0; i < len; i++) {
        buf[i] = soft_spi_transfer_byte(0xFF);
    }

    w25q128_cs_set(1);

    return 0;
}

/**
 * @brief 页编程
 */
int w25q128_page_program(u32 addr, const u8 *buf, u32 len)
{
    if (buf == NULL || len == 0 || len > W25Q128_PAGE_SIZE) {
        log_error("W25Q128: Invalid page program parameters!");
        return -1;
    }

    // 检查页边界
    u32 page_offset = addr % W25Q128_PAGE_SIZE;
    if (page_offset + len > W25Q128_PAGE_SIZE) {
        log_error("W25Q128: Page program crosses page boundary!");
        return -2;
    }

    log_debug("W25Q128: Page program %u bytes to 0x%06x", len, addr);

    // 写使能
    w25q128_write_enable();

    // 等待写使能生效
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

    // 等待写操作完成
    if (w25q128_wait_busy(100) != 0) {
        log_error("W25Q128: Page program timeout!");
        return -4;
    }

    return 0;
}

/**
 * @brief 写入数据 (自动处理页对齐和分页写入)
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
        // 计算当前页的剩余空间
        u32 page_offset = current_addr % W25Q128_PAGE_SIZE;
        u32 page_space = W25Q128_PAGE_SIZE - page_offset;

        // 计算本次写入长度
        u32 write_len = (len - offset) < page_space ? (len - offset) : page_space;

        // 执行页编程
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
 * @brief 扇区擦除 (4KB)
 */
int w25q128_sector_erase(u32 addr)
{
    if (addr % W25Q128_SECTOR_SIZE != 0) {
        log_error("W25Q128: Address not 4KB aligned!");
        return -1;
    }

    log_info("W25Q128: Sector erase at 0x%06x", addr);

    w25q128_write_enable();

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_SECTOR_ERASE_4KB);
    soft_spi_transfer_byte((addr >> 16) & 0xFF);
    soft_spi_transfer_byte((addr >> 8) & 0xFF);
    soft_spi_transfer_byte(addr & 0xFF);
    w25q128_cs_set(1);

    // 等待擦除完成 (典型值: 45ms, 最大值: 400ms)
    if (w25q128_wait_busy(500) != 0) {
        log_error("W25Q128: Sector erase timeout!");
        return -2;
    }

    return 0;
}

/**
 * @brief 块擦除 (32KB)
 */
int w25q128_block_erase_32kb(u32 addr)
{
    if (addr % W25Q128_BLOCK_SIZE_32KB != 0) {
        log_error("W25Q128: Address not 32KB aligned!");
        return -1;
    }

    log_info("W25Q128: Block erase (32KB) at 0x%06x", addr);

    w25q128_write_enable();

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_BLOCK_ERASE_32KB);
    soft_spi_transfer_byte((addr >> 16) & 0xFF);
    soft_spi_transfer_byte((addr >> 8) & 0xFF);
    soft_spi_transfer_byte(addr & 0xFF);
    w25q128_cs_set(1);

    // 等待擦除完成 (典型值: 120ms, 最大值: 1600ms)
    if (w25q128_wait_busy(2000) != 0) {
        log_error("W25Q128: Block erase timeout!");
        return -2;
    }

    return 0;
}

/**
 * @brief 块擦除 (64KB)
 */
int w25q128_block_erase_64kb(u32 addr)
{
    if (addr % W25Q128_BLOCK_SIZE_64KB != 0) {
        log_error("W25Q128: Address not 64KB aligned!");
        return -1;
    }

    log_info("W25Q128: Block erase (64KB) at 0x%06x", addr);

    w25q128_write_enable();

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_BLOCK_ERASE_64KB);
    soft_spi_transfer_byte((addr >> 16) & 0xFF);
    soft_spi_transfer_byte((addr >> 8) & 0xFF);
    soft_spi_transfer_byte(addr & 0xFF);
    w25q128_cs_set(1);

    // 等待擦除完成 (典型值: 150ms, 最大值: 2000ms)
    if (w25q128_wait_busy(2500) != 0) {
        log_error("W25Q128: Block erase timeout!");
        return -2;
    }

    return 0;
}

/**
 * @brief 芯片擦除
 */
int w25q128_chip_erase(void)
{
    log_info("W25Q128: Chip erase started...");

    w25q128_write_enable();

    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_CHIP_ERASE);
    w25q128_cs_set(1);

    // 等待擦除完成 (典型值: 5s, 最大值: 32s)
    if (w25q128_wait_busy(35000) != 0) {
        log_error("W25Q128: Chip erase timeout!");
        return -1;
    }

    log_info("W25Q128: Chip erase completed!");
    return 0;
}

/**
 * @brief 进入掉电模式
 */
void w25q128_power_down(void)
{
    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_POWER_DOWN);
    w25q128_cs_set(1);

    log_info("W25Q128: Power down mode");

    // 等待 tDP (最大 3us)
    spi_delay();
    spi_delay();
    spi_delay();
}

/**
 * @brief 退出掉电模式
 */
void w25q128_release_power_down(void)
{
    w25q128_cs_set(0);
    soft_spi_transfer_byte(W25Q128_RELEASE_POWER_DOWN);
    w25q128_cs_set(1);

    log_info("W25Q128: Release power down");

    // 等待 tRES1 (最小 20us)
    for (int i = 0; i < 20; i++) {
        spi_delay();
    }
}

/**
 * @brief W25Q128 读写测试函数
 */
int w25q128_test(void)
{
    int ret;
    u8 write_buf[512];  // 增加到 512 字节以支持跨页测试
    u8 read_buf[512];
    u8 test_passed = 1;

    printf("\n========================================\n");
    printf("   W25Q128 Flash Read/Write Test\n");
    printf("========================================\n");

    // 0. 简单测试：写入 0x11223344 并读回
    printf("\n[0] Quick Test: Write 0x11223344 and Read Back...\n");
    u32 test_addr = 0x001000;  // 使用 0x1000 地址避免覆盖重要数据

    // 擦除该扇区
    printf("    Erasing sector at 0x%06x...\n", test_addr);
    ret = w25q128_sector_erase(test_addr);
    if (ret != 0) {
        printf("    [FAILED] Sector erase failed!\n");
    } else {
        // 准备 4 字节数据: 0x11 0x22 0x33 0x44
        write_buf[0] = 0x66;
        write_buf[1] = 0x77;
        write_buf[2] = 0x99;
        write_buf[3] = 0x11;

        printf("    Writing 4 bytes: 0x66 0x77 0x99 0x11 to 0x%06x...\n", test_addr);
        ret = w25q128_page_program(test_addr, write_buf, 4);
        if (ret == 0) {
            printf("    Write OK!\n");

            // 读回数据
            printf("    Reading back 4 bytes from 0x%06x...\n", test_addr);
            ret = w25q128_read_data(test_addr, read_buf, 4);
            if (ret == 0) {
                printf("    Read OK!\n");
                printf("    Data read: 0x%02x 0x%02x 0x%02x 0x%02x\n",
                       read_buf[0], read_buf[1], read_buf[2], read_buf[3]);

                // 验证数据
                if (read_buf[0] == 0x66 && read_buf[1] == 0x77 &&
                    read_buf[2] == 0x99 && read_buf[3] == 0x11) {
                    printf("    [PASSED] Data matches! 0x66779911 written and verified!\n");
                } else {
                    printf("    [FAILED] Data mismatch!\n");
                    printf("    Expected: 0x66 0x77 0x99 0x11\n");
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

    // 1. 读取 JEDEC ID
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

    // 2. 读取状态寄存器
    printf("\n[2] Reading Status Registers...\n");
    u8 sr1 = w25q128_read_status_reg(1);
    u8 sr2 = w25q128_read_status_reg(2);
    u8 sr3 = w25q128_read_status_reg(3);
    printf("    SR1: 0x%02x, SR2: 0x%02x, SR3: 0x%02x\n", sr1, sr2, sr3);
    printf("    Busy: %d, WEL: %d\n", (sr1 & 0x01) ? 1 : 0, (sr1 & 0x02) ? 1 : 0);

    // 3. 扇区擦除测试
    printf("\n[3] Erasing Test Sector (4KB at 0x000000)...\n");
    ret = w25q128_sector_erase(0x000000);
    if (ret == 0) {
        printf("    Sector erase OK!\n");
    } else {
        printf("    [FAILED] Sector erase failed!\n");
        test_passed = 0;
    }

    // 4. 准备测试数据
    printf("\n[4] Preparing Test Data...\n");
    for (int i = 0; i < 256; i++) {
        write_buf[i] = (u8)(i * 0x11);  // 0x11, 0x22, 0x33, ..., 0xff (重复)
    }
    printf("    Test pattern: 0x11, 0x22, 0x33, ..., 0xff\n");

    // 5. 写入测试
    printf("\n[5] Writing 256 bytes to address 0x000000...\n");
    ret = w25q128_page_program(0x000000, write_buf, 256);
    if (ret == 0) {
        printf("    Write OK!\n");
    } else {
        printf("    [FAILED] Write failed!\n");
        test_passed = 0;
    }

    // 6. 读取验证
    printf("\n[6] Reading back 256 bytes from address 0x000000...\n");
    ret = w25q128_read_data(0x000000, read_buf, 256);
    if (ret == 0) {
        printf("    Read OK!\n");
    } else {
        printf("    [FAILED] Read failed!\n");
        test_passed = 0;
    }

    // 7. 数据比较
    printf("\n[7] Verifying Data...\n");
    int error_count = 0;
    for (int i = 0; i < 256; i++) {
        if (read_buf[i] != write_buf[i]) {
            if (error_count < 10) {  // 只显示前10个错误
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

    // 8. 显示部分数据
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

    // 9. 跨页写入测试
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

    // 10. 跨页读取验证
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

    // 测试结果
    printf("\n========================================\n");
    if (test_passed) {
        printf("   TEST RESULT: PASSED!\n");
    } else {
        printf("   TEST RESULT: FAILED!\n");
    }
    printf("========================================\n\n");

    return test_passed ? 0 : -1;
}
