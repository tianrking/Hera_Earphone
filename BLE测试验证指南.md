# BLE ae04 Notify 测试验证指南

## 修改内容
已经修改 `le_rcsp_adv_module.c` 文件，使 ae04 characteristic 启用 notify 后只发送测试数据 `11 22 33 44`。

### 1. **代码修改位置**
- 文件：`apps/common/third_party_profile/jieli/JL_rcsp/bt_trans_data/le_rcsp_adv_module.c`

### 2. **主要修改**
```c
case ATT_CHARACTERISTIC_ae04_01_CLIENT_CONFIGURATION_HANDLE:
    if (buffer[0]) {
        opus_mode = true;
        // 注释掉原来的Opus编码逻辑
        // mic_rec_clock_set();
        // audio_mic_enc_open(rec_enc_mic_output, AUDIO_CODING_OPUS, 0 << 6);
        // audio_dec_enc_open(rec_enc_dec_output, AUDIO_CODING_OPUS, 0 << 6);
        can_send_now_wakeup();
        log_info("\n------ae04 notify enabled, sending test data\n");
    }
```

```c
if (opus_mode) {
    // 修改为发送测试数据 11 22 33 44
    u8 test_data[4] = {0x11, 0x22, 0x33, 0x44};

    int ret = app_send_user_data(
        ATT_CHARACTERISTIC_ae04_01_VALUE_HANDLE,
        test_data,
        4,  // 发送4字节的测试数据
        ATT_OP_AUTO_READ_CCC
    );

    if (ret == 0) {
        log_info("test data sent: 11 22 33 44\n");
    }
}
```

## 测试步骤

### 1. **编译和烧录**
```bash
# 清理并编译项目
make clean
make

# 烧录到设备
# 使用相应的烧录工具
```

### 2. **BLE连接测试**

#### 使用手机APP测试
1. 安装BLE调试APP（如：nRF Connect、LightBlue等）
2. 打开APP并扫描设备
3. 连接到设备（设备名通常为 "br22_ble_test" 或自定义名称）
4. 在服务列表中找到包含 `ae04` characteristic 的服务
5. 点击 `ae04` 的 notify/indicate 按钮启用通知
6. 观察是否持续接收到数据 `11 22 33 44`

#### 使用Python测试（示例）
```python
from bluepy.btle import Peripheral, UUID
import time

# 设备MAC地址（需要替换为实际地址）
device_mac = "XX:XX:XX:XX:XX:XX"

# 连接设备
peripheral = Peripheral(device_mac)

try:
    # 获取服务（需要替换为实际的service UUID）
    # service_uuid = UUID("xxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")
    # service = peripheral.getServiceByUUID(service_uuid)

    # 获取ae04 characteristic（需要替换为实际的characteristic UUID）
    # ae04_uuid = UUID("xxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")
    # ae04_char = service.getCharacteristics(ae04_uuid)[0]

    # 启用notify
    # peripheral.writeCharacteristic(ae04_char.getHandle() + 1, b"\x01\x00")

    # 设置通知回调
    def notification_handler(c_handle, data):
        print(f"Received data: {data.hex()}")
        if data == b"\x11\x22\x33\x44":
            print("✓ Test data verified!")

    # 注册通知
    # peripheral.withDelegate(NotificationDelegate(notification_handler))

    # 持续接收数据
    while True:
        peripheral.waitForNotifications(1.0)

except KeyboardInterrupt:
    print("Stopping...")
finally:
    peripheral.disconnect()
```

### 3. **串口调试信息**
通过串口监听，应该能看到以下信息：
```
------write ccc:xxxx, 01
------ae04 notify enabled, sending test data
test data sent: 11 22 33 44
test data sent: 11 22 33 44
...
```

### 4. **验证要点**
1. **BLE连接建立**：确认设备可以被扫描和连接
2. **Notify启用**：确认ae04的notify可以成功启用
3. **数据接收**：确认持续接收到 `11 22 33 44` 数据
4. **无Opus编码**：确认系统没有启动Opus编码器（节省CPU资源）

## 恢复原代码
如果需要恢复原来的Opus编码功能，只需要：
1. 删除注释，恢复原来的代码
2. 重新编译烧录

```c
// 恢复这部分代码
mic_rec_clock_set();
audio_mic_enc_open(rec_enc_mic_output, AUDIO_CODING_OPUS, 0 << 6);
audio_dec_enc_open(rec_enc_dec_output, AUDIO_CODING_OPUS, 0 << 6);

// 恢复原来的数据打包逻辑
memset(opus_packages + opus_idx * OPUS_PACKAGE_BYTE, 0, OPUS_PACKAGE_BYTE);
// ... 其他恢复代码
```

## 常见问题

1. **接收不到数据**
   - 检查 `TEST_SEND_DATA_RATE` 宏是否定义为1
   - 确认BLE连接是否稳定
   - 查看串口输出是否有错误信息

2. **数据发送频率问题**
   - 当前实现会尽可能快地发送数据
   - 可以在 `test_data_send_packet` 中添加延时控制频率

3. **恢复Opus功能后的问题**
   - 确认 `TCFG_ENC_OPUS_ENABLE` 在配置文件中已启用
   - 检查时钟设置是否正确恢复

---
*测试日期: 2025-12-07*