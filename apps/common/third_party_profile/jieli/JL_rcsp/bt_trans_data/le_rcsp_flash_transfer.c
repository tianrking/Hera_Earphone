//================= Flash高速传输实现 ===============

// 修改开始
#include "app_config.h"
#include "system/timer.h"
#include "system/event.h"
#include "norflash.h"
#include "rcsp_user_update.h"
#include "le_rcsp_adv_module.h"

// Flash传输缓冲区
#define FLASH_BUFFER_SIZE       2048
#define MAX_PACKET_SIZE         244    // MTU - 3字节ATT头
#define TRANSFER_TIMER_MS       5      // 5ms发送间隔

// 数据包结构
typedef struct {
    u16 sequence;     // 序列号
    u16 checksum;     // CRC16校验
    u8 data[MAX_PACKET_SIZE - 4];  // 实际数据
} __attribute__((packed)) data_packet_t;

// 传输状态管理
typedef struct {
    u32 total_size;        // 总大小
    u32 sent_size;         // 已发送大小
    u32 flash_addr;        // Flash起始地址
    u32 current_read_pos;  // 当前读取位置
    u8 buffer[FLASH_BUFFER_SIZE];  // 缓冲区
    u16 buffer_used;       // 缓冲区已使用
    u16 buffer_read_pos;   // 缓冲区读取位置
    bool is_active;        // 传输是否激活
    u16 packet_sequence;   // 包序列号
    u32 timer_handle;      // 定时器句柄
} flash_transfer_t;

static flash_transfer_t g_flash_tx = {0};

// CRC16计算表
static const u16 crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    // ... 完整的CRC16表（省略部分）
};

// CRC16计算
static u16 calculate_crc16(const u8 *data, u16 len)
{
    u16 crc = 0;
    while (len--) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ *data++];
    }
    return crc;
}

// 从Flash读取数据到缓冲区
static int read_flash_data(void)
{
    if (g_flash_tx.current_read_pos >= g_flash_tx.flash_addr + g_flash_tx.total_size) {
        return 0;  // 读取完成
    }

    u32 remaining = (g_flash_tx.flash_addr + g_flash_tx.total_size) - g_flash_tx.current_read_pos;
    u32 read_size = (remaining > FLASH_BUFFER_SIZE) ? FLASH_BUFFER_SIZE : remaining;

    int ret = norflash_read(NULL, g_flash_tx.buffer, read_size, g_flash_tx.current_read_pos);
    if (ret == 0) {
        g_flash_tx.buffer_used = read_size;
        g_flash_tx.buffer_read_pos = 0;
        g_flash_tx.current_read_pos += read_size;
        log_info("Flash read: addr=0x%x, size=%d\n", g_flash_tx.current_read_pos - read_size, read_size);
        return read_size;
    }

    log_info("Flash read error: %d\n", ret);
    return -1;
}

// 发送一个数据包
static int send_data_packet(void)
{
    // 检查缓冲区是否需要重新填充
    if (g_flash_tx.buffer_read_pos >= g_flash_tx.buffer_used) {
        int ret = read_flash_data();
        if (ret <= 0) {
            // 传输完成或出错
            g_flash_tx.is_active = false;
            log_info("Flash transfer %s\n", ret == 0 ? "completed" : "failed");
            return 0;
        }
    }

    // 准备数据包
    data_packet_t packet;
    u16 data_size = g_flash_tx.buffer_used - g_flash_tx.buffer_read_pos;
    if (data_size > sizeof(packet.data)) {
        data_size = sizeof(packet.data);
    }

    packet.sequence = g_flash_tx.packet_sequence++;
    memcpy(packet.data, g_flash_tx.buffer + g_flash_tx.buffer_read_pos, data_size);
    packet.checksum = calculate_crc16(packet.data, data_size);

    // 发送数据包
    int ret = app_send_user_data(
        ATT_CHARACTERISTIC_ae04_01_VALUE_HANDLE,
        (u8 *)&packet,
        sizeof(packet.sequence) + sizeof(packet.checksum) + data_size,
        ATT_OP_AUTO_READ_CCC
    );

    if (ret == 0) {
        g_flash_tx.buffer_read_pos += data_size;
        g_flash_tx.sent_size += data_size;

        // 每100包打印进度
        if ((g_flash_tx.packet_sequence % 100) == 0) {
            float progress = (float)g_flash_tx.sent_size / g_flash_tx.total_size * 100;
            log_info("Transfer progress: %.1f%% (%d/%d bytes)\n",
                    progress, g_flash_tx.sent_size, g_flash_tx.total_size);
        }
    } else {
        log_info("Send failed: %d, retry later\n", ret);
    }

    return ret;
}

// 定时发送处理函数
static void flash_transfer_timer_handler(void)
{
    if (!g_flash_tx.is_active) {
        return;
    }

    // 确保BLE连接正常
    extern hci_con_handle_t con_handle;
    if (!con_handle) {
        log_info("BLE disconnected, stop transfer\n");
        stop_flash_transfer();
        return;
    }

    send_data_packet();
}

// 请求BLE连接参数更新（最小间隔以获得最大速度）
static void request_fast_connection_params(void)
{
    extern hci_con_handle_t con_handle;
    if (!con_handle) {
        return;
    }

    // 请求最小连接间隔：7.5ms (6 * 1.25ms)
    u16 min_interval = 6;
    u16 max_interval = 6;
    u16 latency = 0;        // 无延迟
    u16 timeout = 50;       // 500ms超时

    ble_user_cmd_prepare(BLE_CMD_CONNECTION_UPDATE_REQ, 5,
                        con_handle, min_interval, max_interval,
                        latency, timeout);

    log_info("Requested fast connection params: 7.5ms interval\n");
}

// 启动Flash传输
int start_flash_transfer(u32 flash_addr, u32 size)
{
    if (size == 0 || g_flash_tx.is_active) {
        log_info("Invalid transfer or already active\n");
        return -1;
    }

    // 初始化传输状态
    memset(&g_flash_tx, 0, sizeof(g_flash_tx));
    g_flash_tx.total_size = size;
    g_flash_tx.flash_addr = flash_addr;
    g_flash_tx.current_read_pos = flash_addr;
    g_flash_tx.is_active = true;

    // 请求快速连接参数
    request_fast_connection_params();

    // 启动发送定时器
    g_flash_tx.timer_handle = sys_timer_add(NULL, flash_transfer_timer_handler, TRANSFER_TIMER_MS);
    if (!g_flash_tx.timer_handle) {
        log_info("Failed to create transfer timer\n");
        g_flash_tx.is_active = false;
        return -1;
    }

    log_info("Flash transfer started: addr=0x%x, size=%d bytes\n", flash_addr, size);
    return 0;
}

// 停止Flash传输
int stop_flash_transfer(void)
{
    if (!g_flash_tx.is_active) {
        return 0;
    }

    g_flash_tx.is_active = false;

    if (g_flash_tx.timer_handle) {
        sys_timer_del(g_flash_tx.timer_handle);
        g_flash_tx.timer_handle = 0;
    }

    log_info("Flash transfer stopped at %d bytes\n", g_flash_tx.sent_size);
    return 0;
}

// 获取传输进度
void get_transfer_progress(u32 *sent, u32 *total, float *percentage)
{
    if (sent) *sent = g_flash_tx.sent_size;
    if (total) *total = g_flash_tx.total_size;
    if (percentage) {
        *percentage = g_flash_tx.total_size > 0 ?
                     (float)g_flash_tx.sent_size / g_flash_tx.total_size * 100 : 0;
    }
}

// 接收端ACK处理（如果实现了ACK机制）
void handle_transfer_ack(u16 sequence)
{
    // 可以在这里实现选择性重传
    log_info("Received ACK for sequence: %d\n", sequence);
}

// 导出函数供外部调用
// 在 le_rcsp_adv_module.c 中添加以下声明：
// extern int start_flash_transfer(u32 flash_addr, u32 size);
// extern int stop_flash_transfer(void);
// extern void get_transfer_progress(u32 *sent, u32 *total, float *percentage);

// 修改结束
================= Flash高速传输实现 ===============