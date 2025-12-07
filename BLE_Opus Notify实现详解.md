# BLE Opus Notify 实现详解

## 概述
本文档详细分析Hera耳机如何通过BLE的Notify机制发送Opus编码的音频数据。

## 关键文件
- **核心实现**: [`le_rcsp_adv_module.c`](apps/common/third_party_profile/jieli/JL_rcsp/bt_trans_data/le_rcsp_adv_module.c)
- **配置定义**: [`fft_and_pca.h`](cpu/br28/fft_and_pca.h)

## BLE特征值（Characteristics）

### 1. **特征值定义**
- **ae03** (PCA模式): `ATT_CHARACTERISTIC_ae03_01_CLIENT_CONFIGURATION_HANDLE`
- **ae04** (Opus模式): `ATT_CHARACTERISTIC_ae04_01_CLIENT_CONFIGURATION_HANDLE`

### 2. **触发机制**
在 [`att_write_callback()`](apps/common/third_party_profile/jieli/JL_rcsp/bt_trans_data/le_rcsp_adv_module.c#L705) 中：

```c
// 当客户端启用ae03的notify时 -> 启动PCA压缩模式
case ATT_CHARACTERISTIC_ae03_01_CLIENT_CONFIGURATION_HANDLE:
    if (buffer[0]) {
        opus_mode = false;
        transcription_open();    // 启动PCA压缩
        can_send_now_wakeup();   // 开始发送数据
    }

// 当客户端启用ae04的notify时 -> 启动OPUS压缩模式
case ATT_CHARACTERISTIC_ae04_01_CLIENT_CONFIGURATION_HANDLE:
    if (buffer[0]) {
        opus_mode = true;
        mic_rec_clock_set();    // 设置时钟为160MHz
        // 打开Opus编码器（麦克风）
        audio_mic_enc_open(rec_enc_mic_output, AUDIO_CODING_OPUS, 0 << 6);
        // 打开Opus编码器（解码器/参考音频）
        audio_dec_enc_open(rec_enc_dec_output, AUDIO_CODING_OPUS, 0 << 6);
        can_send_now_wakeup();   // 开始发送数据
    }
```

## Opus数据包结构

### 1. **数据包格式定义**
```c
#define OPUS_PART_BYTE 40       // 每部分音频数据大小
#define OPUS_PACKAGE_BYTE 84    // 整个包大小
#define MAX_CONFLICT_COUNT 5    // 缓冲区数量
```

### 2. **包结构详解**
在 [`test_data_send_packet()`](apps/common/third_party_profile/jieli/JL_rcsp/bt_trans_data/le_rcsp_adv_module.c#L314) 中组装数据包：

```
字节偏移    内容                    大小
0           VAD激活状态            1字节
1-40        麦克风Opus数据         40字节
41-80       解码器Opus数据         40字节
81-83       发送索引计数器         3字节（DEBUG_BYTE）
```

### 3. **数据包组装代码**
```c
void test_data_send_packet(void)
{
    if (opus_mode) {
        // 清零数据包
        memset(opus_packages + opus_idx * OPUS_PACKAGE_BYTE, 0, OPUS_PACKAGE_BYTE);

        // 第0字节：VAD状态
        opus_packages[opus_idx * OPUS_PACKAGE_BYTE] = vad_is_activate;

        // 字节1-40：麦克风数据（如果未发送）
        if (!opus_mic_buffer_sent) {
            memcpy(opus_packages + opus_idx * OPUS_PACKAGE_BYTE + 1,
                   opus_mic_buffer, OPUS_PART_BYTE);
        }

        // 字节41-80：解码器数据（如果未发送）
        if (!opus_dec_buffer_sent) {
            memcpy(opus_packages + opus_idx * OPUS_PACKAGE_BYTE + 1 + OPUS_PART_BYTE,
                   opus_dec_buffer, OPUS_PART_BYTE);
        }

        // 最后3字节：发送索引（用于调试）
        opus_packages[(opus_idx + 1) * OPUS_PACKAGE_BYTE - DEBUG_BYTE] = send_index;

        // 通过BLE发送
        app_send_user_data(ATT_CHARACTERISTIC_ae04_01_VALUE_HANDLE,
                          opus_packages + opus_idx * OPUS_PACKAGE_BYTE,
                          OPUS_PACKAGE_BYTE,
                          ATT_OP_AUTO_READ_CCC);
    }
}
```

## 数据流处理

### 1. **Opus编码器回调**

#### 麦克风数据回调
```c
static int rec_enc_mic_output(void *priv, void *buf, int len)
{
    bt_sniff_ready_clean();  // 清除蓝牙sniff状态

    // 将编码后的Opus数据存入缓冲区
    memcpy(opus_mic_buffer, (u8 *)buf, len);
    opus_mic_buffer_sent = false;  // 标记数据未发送

    return 0;
}
```

#### 解码器数据回调
```c
static int rec_enc_dec_output(void *priv, void *buf, int len)
{
    bt_sniff_ready_clean();  // 清除蓝牙sniff状态

    // 将编码后的Opus数据存入缓冲区
    memcpy(opus_dec_buffer, (u8 *)buf, len);
    opus_dec_buffer_sent = false;  // 标记数据未发送

    return 0;
}
```

### 2. **时钟管理**
为确保编码性能，系统会提升时钟频率：
```c
void mic_rec_clock_set(void)
{
    sys_clk_before_rec = clk_get("sys");        // 保存当前时钟
    clk_set("sys", AUDIO_ENC_SYS_CLK_HZ);      // 设置为160MHz
    clk_set_en(0);                             // 禁用时钟门控
}
```

## 缓冲区管理

### 1. **缓冲区定义**
```c
// Opus数据包缓冲区（5个包的环形缓冲区）
u8 opus_packages[OPUS_PACKAGE_BYTE * MAX_CONFLICT_COUNT];

// 单个音频帧缓冲区
u8 opus_mic_buffer[OPUS_PART_BYTE];   // 麦克风数据
u8 opus_dec_buffer[OPUS_PART_BYTE];   // 解码器数据

// 发送状态标志
bool opus_mic_buffer_sent;
bool opus_dec_buffer_sent;
```

### 2. **发送流程**
1. Opus编码器产生压缩数据（40字节）
2. 数据存入 `opus_mic_buffer` 或 `opus_dec_buffer`
3. 标记对应的 `buffer_sent` 为false
4. `test_data_send_packet()` 定期检查并发送
5. 发送成功后更新索引，失败则重试

## 性能参数

| 参数 | 值 | 说明 |
|-----|-----|-----|
| Opus帧大小 | 40字节 | 压缩后的音频数据 |
| BLE包大小 | 84字节 | 包含VAD、MIC、DEC数据 |
| 采样率 | 16kHz | 原始音频采样率 |
| 时钟频率 | 160MHz | Opus编码时的系统时钟 |
| 缓冲深度 | 5包 | 平滑数据传输 |
| 发送周期 | 约10ms | 根据Opus帧长决定 |

## 使用场景

### 1. **双通道音频传输**
- **麦克风通道**: 实时录音的Opus压缩数据
- **解码器通道**: 可能是参考音频或立体声的另一通道

### 2. **VAD集成**
- 每个数据包包含VAD（语音活动检测）状态
- 接收端可以根据VAD状态优化处理

### 3. **调试支持**
- 包含3字节的发送索引
- 可用于检测丢包和排序

## 与标准方案的对比

| 特性 | BLE Opus方案 | LC3无线麦克风方案 |
|-----|-------------|----------------|
| 编码格式 | Opus | LC3 |
| 包大小 | 84字节 | 可配置 |
| 双通道 | 支持 | 单通道 |
| VAD集成 | 内置 | 无 |
| 调试功能 | 支持 | 无 |

## 总结

Hera耳机的BLE Opus实现是一个完整的双向音频传输方案：

1. **灵活的触发机制**：通过启用不同的Characteristic来选择工作模式
2. **高效的数据打包**：将多个音频流合并到一个BLE包中
3. **完善的缓冲管理**：使用环形缓冲区平滑数据流
4. **性能优化**：动态调整系统时钟确保编码性能
5. **调试友好**：包含VAD状态和发送索引

该方案特别适用于需要高质量、低延迟音频传输的应用，如语音助手、实时翻译等场景。

---
*更新日期: 2025-12-07*