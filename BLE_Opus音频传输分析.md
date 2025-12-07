# Hera耳机BLE Opus音频传输分析

## 概述
Hera耳机支持通过BLE发送Opus编码的音频数据。本文档分析完整的实现流程，从麦克风采集到BLE发送。

## 核心配置

### 1. **配置文件**
在 [`board_jl7016g_hybrid_cfg.h:1018`](apps/earphone/board/br28/board_jl7016g_hybrid_cfg.h#L1018)：
```c
#define TCFG_ENC_OPUS_ENABLE                ENABLE
```

其他配置文件的状态：
- `board_jl701n_demo_cfg.h`: DISABLE
- `board_jl701n_btemitter_cfg.h`: DISABLE

### 2. **编码格式支持**
系统同时支持多种编码格式：
```c
#define TCFG_ENC_MSBC_ENABLE                ENABLE
#define TCFG_ENC_CVSD_ENABLE                ENABLE
#define TCFG_ENC_OPUS_ENABLE                ENABLE    // Opus编码
```

## 数据流程

### 1. **完整的音频数据流**
```
麦克风ADC采集 → audio_mic_enc_open() → Opus编码 → speech_data_send() → BLE发送
```

### 2. **关键函数调用链**

#### Opus编码器初始化
在 [`audio_mic_codec.c:165`](cpu/br28/audio_common/audio_mic_codec.c#L165)：
```c
int audio_mic_enc_open(int (*mic_output)(void *priv, void *buf, int len),
                       u32 code_type, u8 ai_type)
{
    switch (code_type) {
    case AUDIO_CODING_OPUS:
        fmt.quality = 0 | ai_type;  // 质量设置
        fmt.sample_rate = 16000;     // 16kHz采样率
        fmt.coding_type = AUDIO_CODING_OPUS;
        break;
    // ...
    }
}
```

#### Opus编码参数说明
```c
// quality参数组成：
// bit7-bit6: 格式模式 (0:百度_无头, 1:酷狗_eng+range)
// bit5-bit4: 复杂度 (0:高复杂度高质量, 1:低复杂度低质量)
// bit3-bit0: 比特率 (0:16kbps, 1:32kbps, 2:64kbps)
```

#### 数据输出回调
在 [`mic_rec.c:221`](apps/common/third_party_profile/common/mic_rec.c#L221)：
```c
static int rec_enc_output(void *priv, void *buf, int len)
{
    // 1. 清除蓝牙sniff状态
    bt_sniff_ready_clean();

    // 2. 通过speech_data_send发送数据
    if (speech_data_send(buf, len, __this->ai_enc_info.sender) == (u16)(-1)) {
        log_info("opus data miss !!! line:%d \n", __LINE__);
    }

    return 0;
}
```

#### 数据缓冲和发送
在 [`mic_rec.c:90`](apps/common/third_party_profile/common/mic_rec.c#L90)：
```c
static u16 speech_data_send(u8 *buf, u16 len, u16(*send_data)(u8 *buf, u16 len))
{
    // 1. 写入环形缓冲区
    if (cbuf_write(&(__this->buf_ctl.cbuffer), buf, len) != len) {
        res = (u16)(-1);
    }

    // 2. 当缓冲区数据足够时，通过send_data回调发送
    while (cbuf_get_data_size(&(__this->buf_ctl.cbuffer)) >= send_len) {
        cbuf_read_alloc_len(&(__this->buf_ctl.cbuffer), temp_buf, send_len);
        if (send_data) {
            if (!send_data(temp_buf, send_len)) {
                // 发送成功
                cbuf_read_alloc_len_updata(&(__this->buf_ctl.cbuffer), send_len);
            }
        }
    }
}
```

### 3. **BLE发送接口**
根据配置，`send_data`回调指向实际的数据发送函数，可能包括：
- `dma_speech_data_send` - DMA方式发送
- `XM_speech_data_send` - 小米协议发送
- 其他自定义发送函数

## 关键文件位置

| 文件路径 | 功能描述 |
|---------|---------|
| [`cpu/br28/audio_common/audio_mic_codec.c`](cpu/br28/audio_common/audio_mic_codec.c) | Opus编码器实现 |
| [`apps/common/third_party_profile/common/mic_rec.c`](apps/common/third_party_profile/common/mic_rec.c) | 录音控制和数据管理 |
| [`apps/earphone/board/br28/board_jl7016g_hybrid_cfg.h`](apps/earphone/board/br28/board_jl7016g_hybrid_cfg.h) | 配置文件（启用Opus） |

## 配置和使用

### 1. **启用Opus编码**
在配置文件中设置：
```c
#define TCFG_ENC_OPUS_ENABLE    ENABLE
```

### 2. **初始化录音**
```c
// 初始化参数
mic_rec_pram_init(AUDIO_CODING_OPUS, opus_type, send_func, frame_num, cbuf_size);

// 开始录音
ai_mic_rec_open();
```

### 3. **停止录音**
```c
ai_mic_rec_close();
```

## 性能参数

- **采样率**: 16kHz
- **编码格式**: Opus
- **支持比特率**: 16kbps, 32kbps, 64kbps
- **缓冲区大小**: 可配置（frame_num * frame_size）
- **编码复杂度**: 可选（高复杂度/低复杂度）

## 注意事项

1. **资源冲突**: 使用Opus编码时会暂停A2DP解码
2. **TWS支持**: 支持TWS双耳同步传输
3. **功耗管理**: 会清除蓝牙sniff状态以保证实时性
4. **缓冲区管理**: 使用环形缓冲区管理数据流

## 与LC3编码的区别

| 特性 | LC3编码 | Opus编码 |
|-----|---------|----------|
| 应用场景 | LE Audio标准 | 第三方协议 |
| 延迟 | 超低延迟 | 低延迟 |
| 兼容性 | LE Audio标准设备 | 需要特定支持 |
| 配置文件 | 无需额外配置 | 需要启用TCFG_ENC_OPUS_ENABLE |

## 总结

Hera耳机的Opus编码通过BLE发送是一个完整的音频传输方案：
1. ADC采集麦克风数据
2. 使用硬件加速的Opus编码器压缩
3. 通过缓冲区管理平滑数据流
4. 使用BLE GATT的Write Without Response或Notify发送
5. 支持多种第三方协议（如小米协议）

该方案适用于需要高质量音频传输的应用场景，如语音助手、语音通话等。

---
*更新日期: 2025-12-07*