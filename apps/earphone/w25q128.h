/**
 * @file w25q128.h
 * @brief W25Q128 Flash software SPI driver.
 * @note Default board wiring uses 3-wire SPI: CS=PC3, CLK=PC4, IO=PC5.
 * @note SDPG/VDD is driven by PE6; PG8 is kept as input on the shared pad.
 */

#ifndef __W25Q128_H__
#define __W25Q128_H__

#include "typedef.h"
#include "gpio.h"
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Pin Configuration ==================== */
#ifndef W25Q128_3WIRE_ENABLE
#define W25Q128_3WIRE_ENABLE              1
#endif

#ifndef W25Q128_IO_PIN
#define W25Q128_IO_PIN                    IO_PORTC_05
#endif

#ifndef W25Q128_CS_PIN
#define W25Q128_CS_PIN                    IO_PORTC_03
#endif

#ifndef W25Q128_CLK_PIN
#define W25Q128_CLK_PIN                   IO_PORTC_04
#endif

#ifndef W25Q128_POWER_PIN
#define W25Q128_POWER_PIN                 IO_PORTE_06
#endif

#ifndef W25Q128_POWER_SENSE_PIN
#define W25Q128_POWER_SENSE_PIN           IO_PORTG_08
#endif

/* ==================== W25Q128 Commands ==================== */
#define W25Q128_WRITE_ENABLE              0x06
#define W25Q128_WRITE_DISABLE             0x04
#define W25Q128_READ_STATUS_REG1          0x05
#define W25Q128_READ_STATUS_REG2          0x35
#define W25Q128_READ_STATUS_REG3          0x15
#define W25Q128_WRITE_STATUS_REG          0x01
#define W25Q128_PAGE_PROGRAM              0x02
#define W25Q128_QUAD_PAGE_PROGRAM         0x32
#define W25Q128_BLOCK_ERASE_64KB          0xD8
#define W25Q128_BLOCK_ERASE_32KB          0x52
#define W25Q128_SECTOR_ERASE_4KB          0x20
#define W25Q128_CHIP_ERASE                0xC7
#define W25Q128_ERASE_SUSPEND             0x75
#define W25Q128_ERASE_RESUME              0x7A
#define W25Q128_POWER_DOWN                0xB9
#define W25Q128_RELEASE_POWER_DOWN        0xAB
#define W25Q128_READ_DATA                 0x03
#define W25Q128_FAST_READ                 0x0B
#define W25Q128_FAST_READ_DUAL_OUTPUT     0x3B
#define W25Q128_JEDEC_ID                  0x9F
#define W25Q128_READ_UNIQUE_ID            0x4B
#define W25Q128_READ_SFDP                 0x5A

/* ==================== W25Q128 Geometry ==================== */
#define W25Q128_PAGE_SIZE                 256
#define W25Q128_SECTOR_SIZE               4096
#define W25Q128_BLOCK_SIZE_32KB           32768
#define W25Q128_BLOCK_SIZE_64KB           65536
#define W25Q128_CHIP_SIZE                 (16 * 1024 * 1024)

/* Status register bits */
#define W25Q128_SR_BUSY_MASK              0x01
#define W25Q128_SR_WEL_MASK               0x02
#define W25Q128_SR_BP_MASK                0x1C
#define W25Q128_SR_TB_MASK                0x20
#define W25Q128_SR_SRP_MASK               0x80

int w25q128_init(void);
void w25q128_deinit(void);
int w25q128_read_jedec_id(u8 *manuf_id, u16 *device_id);
u8 w25q128_read_status_reg(u8 reg_num);
int w25q128_wait_busy(u32 timeout_ms);
void w25q128_write_enable(void);
void w25q128_write_disable(void);
int w25q128_read_data(u32 addr, u8 *buf, u32 len);
int w25q128_page_program(u32 addr, const u8 *buf, u32 len);
int w25q128_write_data(u32 addr, const u8 *buf, u32 len);
int w25q128_sector_erase(u32 addr);
int w25q128_block_erase_32kb(u32 addr);
int w25q128_block_erase_64kb(u32 addr);
int w25q128_chip_erase(void);
void w25q128_power_down(void);
void w25q128_release_power_down(void);
int w25q128_test(void);

#ifdef __cplusplus
}
#endif

#endif /* __W25Q128_H__ */
