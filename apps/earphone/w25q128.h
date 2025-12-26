/**
 * @file w25q128.h
 * @brief W25Q128 Flash 芯片软件 SPI 驱动头文件
 * @note 引脚定义: MOSI=PG0, MISO=PG1, CS=PB2, CLK=PB1
 * @note SPI 频率: 1MHz
 */

#ifndef __W25Q128_H__
#define __W25Q128_H__

#include "typedef.h"
#include "gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 引脚定义 ==================== */
// SPI 引脚定义
#define W25Q128_MOSI_PIN    IO_PORTG_00      // MOSI - 主机输出从机输入
#define W25Q128_MISO_PIN    IO_PORTG_01      // MISO - 主机输入从机输出
#define W25Q128_CS_PIN      IO_PORTB_02      // CS   - 片选信号
#define W25Q128_CLK_PIN     IO_PORTB_01      // CLK  - 时钟信号

/* ==================== W25Q128 命令定义 ==================== */
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

/* ==================== W25Q128 参数定义 ==================== */
#define W25Q128_PAGE_SIZE                 256           // 页大小: 256 字节
#define W25Q128_SECTOR_SIZE               4096          // 扇区大小: 4KB
#define W25Q128_BLOCK_SIZE_32KB           32768         // 块大小: 32KB
#define W25Q128_BLOCK_SIZE_64KB           65536         // 块大小: 64KB
#define W25Q128_CHIP_SIZE                 (16*1024*1024)// 芯片容量: 16MB

/* 状态寄存器位定义 */
#define W25Q128_SR_BUSY_MASK              0x01          // BUSY 位 (bit 0)
#define W25Q128_SR_WEL_MASK               0x02          // WEL 位 (bit 1)
#define W25Q128_SR_BP_MASK                0x1C          // BP 块保护位 (bit 2-4)
#define W25Q128_SR_TB_MASK                0x20          // TB 位 (bit 5)
#define W25Q128_SR_SRP_MASK               0x80          // SRP 位 (bit 7)

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化软件 SPI 接口和 W25Q128
 * @return 0-成功, 负值-失败
 */
int w25q128_init(void);

/**
 * @brief 反初始化 W25Q128，释放引脚资源
 */
void w25q128_deinit(void);

/**
 * @brief 读取 JEDEC ID (制造商ID + 设备ID)
 * @param manuf_id 制造商ID (输出)
 * @param device_id 设备ID (输出)
 * @return 0-成功, 负值-失败
 */
int w25q128_read_jedec_id(u8 *manuf_id, u16 *device_id);

/**
 * @brief 读取状态寄存器
 * @param reg_num 寄存器编号 (1/2/3)
 * @return 状态寄存器值
 */
u8 w25q128_read_status_reg(u8 reg_num);

/**
 * @brief 等待 Flash 就绪 (等待 BUSY 位清零)
 * @param timeout_ms 超时时间(毫秒)
 * @return 0-成功, 负值-超时
 */
int w25q128_wait_busy(u32 timeout_ms);

/**
 * @brief 写使能
 */
void w25q128_write_enable(void);

/**
 * @brief 写禁止
 */
void w25q128_write_disable(void);

/**
 * @brief 读取 Flash 数据
 * @param addr 读取地址 (24位地址)
 * @param buf 数据缓冲区
 * @param len 读取长度
 * @return 0-成功, 负值-失败
 */
int w25q128_read_data(u32 addr, u8 *buf, u32 len);

/**
 * @brief 页编程 (写入数据到 Flash，最多256字节)
 * @param addr 写入地址 (24位地址，需要页对齐)
 * @param buf 数据缓冲区
 * @param len 写入长度 (最大256字节)
 * @return 0-成功, 负值-失败
 */
int w25q128_page_program(u32 addr, const u8 *buf, u32 len);

/**
 * @brief 写入数据 (自动处理页对齐和分页写入)
 * @param addr 写入地址 (24位地址)
 * @param buf 数据缓冲区
 * @param len 写入长度
 * @return 0-成功, 负值-失败
 */
int w25q128_write_data(u32 addr, const u8 *buf, u32 len);

/**
 * @brief 扇区擦除 (4KB)
 * @param addr 擦除地址 (需要4KB对齐)
 * @return 0-成功, 负值-失败
 */
int w25q128_sector_erase(u32 addr);

/**
 * @brief 块擦除 (32KB)
 * @param addr 擦除地址 (需要32KB对齐)
 * @return 0-成功, 负值-失败
 */
int w25q128_block_erase_32kb(u32 addr);

/**
 * @brief 块擦除 (64KB)
 * @param addr 擦除地址 (需要64KB对齐)
 * @return 0-成功, 负值-失败
 */
int w25q128_block_erase_64kb(u32 addr);

/**
 * @brief 芯片擦除
 * @return 0-成功, 负值-失败
 */
int w25q128_chip_erase(void);

/**
 * @brief 进入掉电模式
 */
void w25q128_power_down(void);

/**
 * @brief 退出掉电模式
 */
void w25q128_release_power_down(void);

/**
 * @brief W25Q128 读写测试函数
 * @return 0-测试通过, 负值-测试失败
 */
int w25q128_test(void);

#ifdef __cplusplus
}
#endif

#endif /* __W25Q128_H__ */
