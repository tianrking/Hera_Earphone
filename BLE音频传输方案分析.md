# BLE音频传输方案分析

## BLE传输速率限制

### 1. 理论速率
- **物理层速率**: BLE 4.0/4.1 = 1Mbps, BLE 5.0 = 2Mbps
- **MTU大小**: 247字节（代码中定义: ATT_LOCAL_MTU_SIZE）
- **实际有效载荷**: 247 - 3 = 244字节（减去ATT头）

### 2. 实际传输速率

#### 当前设置（代码分析）
```c
#define ATT_LOCAL_MTU_SIZE 247
#define OPUS_PACKAGE_BYTE 84  // 原始音频包大小
#define OPUS_PART_BYTE 40     // 单通道Opus数据
```

#### 速率计算
| 连接间隔 | 每包字节数 | 传输速率 | 适用场景 |
|---------|-----------|---------|---------|
| 7.5ms   | 244字节   | 32.5KB/s (260 kbps) | 最大速率 |
| 15ms    | 244字节   | 16.3KB/s (130 kbps) | 高质量 |
| 30ms    | 244字节   | 8.1KB/s (65 kbps)   | 标准 |
| 100ms   | 244字节   | 2.4KB/s (19 kbps)   | 低功耗 |

## 音频传输方案

### 方案1：高质量Opus音频
- **采样率**: 16kHz
- **比特率**: 32-64 kbps
- **包大小**: 40字节/包（单通道）
- **发送间隔**: 20ms
- **实际速率**: 2KB/s (16 kbps)
- **延迟**: ~50ms
- **音质**: 语音通话级别

### 方案2：双通道立体声
- **采样率**: 16kHz
- **比特率**: 64 kbps × 2 = 128 kbps
- **包大小**: 84字节/包（双通道，代码中原设计）
- **发送间隔**: 20ms
- **实际速率**: 4.2KB/s (34 kbps)
- **延迟**: ~60ms
- **音质**: 音乐级别

### 方案3：低延迟音频
- **采样率**: 8kHz
- **比特率**: 16 kbps
- **包大小**: 20字节/包
- **发送间隔**: 10ms
- **实际速率**: 2KB/s (16 kbps)
- **延迟**: ~30ms
- **音质**: 基础语音

## 实现建议

### 1. 使用现有Opus框架
代码中已经实现了Opus编码：
```c
// 在 le_rcsp_adv_module.c 中
audio_mic_enc_open(rec_enc_mic_output, AUDIO_CODING_OPUS, 0 << 6);
```

### 2. 优化传输参数
```c
// 减小发送间隔，提高实时性
data_send_timer_handle = sys_timer_add(NULL, data_send_timer_handler, 10); // 10ms

// 使用最大MTU
app_send_user_data(handle, audio_data, packet_size, ATT_OP_AUTO_READ_CCC);
```

### 3. 音频缓冲管理
```c
// 环形缓冲区平滑数据流
#define BUFFER_SIZE 4096
static u8 audio_buffer[BUFFER_SIZE];
static u16 read_ptr = 0, write_ptr = 0;
```

## 数据流传输建议

### 非音频数据传输
如果是传输一般数据流：
- **最大吞吐量**: 32.5KB/s
- **推荐包大小**: 200-240字节
- **发送间隔**: 7.5-15ms
- **可靠性**: 使用ACK模式或应用层确认

## 性能对比

| 传输方式 | 速率 | 延迟 | 功耗 | 复杂度 |
|---------|------|------|------|--------|
| BLE Notify | 中等 | 低 | 低 | 简单 |
| BLE Write Without Response | 高 | 低 | 低 | 中等 |
| BLE L2CAP CoC | 高 | 中等 | 中等 | 复杂 |
| 经典蓝牙A2DP | 很高 | 中等 | 高 | 复杂 |

## 结论

BLE可以传输音频流，但需要权衡：
- **✅ 可行**: 语音级别音频完全可行
- **✅ 已支持**: 代码中有完整的Opus实现
- **⚠️ 限制**: 高质量音乐需要BLE 5.0以上
- **💡 建议**: 使用Opus编码，优化发送间隔

---
*更新日期: 2025-12-07*