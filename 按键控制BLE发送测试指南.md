# 按键控制BLE发送测试指南

## 功能概述
通过PA6和PA7按键控制BLE发送的内容：
- **PA6按键**：计数器加1（发送 "Hera + N"）
- **PA7按键**：计数器减1（发送 "Hera + N"）
- **初始值**：0（发送 "Hera 0"）
- **范围**：0-99

## 代码修改总结

### 1. **app_main.c 修改**
- 添加PA7按键支持
- 实现全局计数器 `ble_counter`
- PA6按下时计数器+1
- PA7按下时计数器-1
- 调用 `set_ble_counter_value()` 更新BLE发送内容

### 2. **le_rcsp_adv_module.c 修改**
- 添加 `current_ble_counter` 和 `counter_data` 全局变量
- 实现 `set_ble_counter_value()` 函数
- 修改 `test_data_send_packet()` 发送格式化字符串
- 发送格式：`"Hera + 数字"`

## 测试步骤

### 1. **编译烧录**
```bash
make clean
make
# 烧录到设备
```

### 2. **测试流程**
1. 设备上电，串口会显示：
   ```
   PA6/PA7 key task started, counter: 0
   ```

2. 使用BLE调试APP（如nRF Connect）连接设备

3. 启用ae04 characteristic的notify

4. 观察接收到的数据：
   - 初始：`Hera 0`
   - 按PA6：`Hera 1` → `Hera 2` → `Hera 3` ...
   - 按PA7：`Hera 3` → `Hera 2` → `Hera 1` → `Hera 0`

### 3. **串口调试信息**
```
PA6 Pressed! Counter: 1
BLE counter updated: Hera 1
BLE data sent: Hera 1 (len=6)

PA7 Pressed! Counter: 0
BLE counter updated: Hera 0
BLE data sent: Hera 0 (len=6)
```

## 硬件连接

| 引脚 | 功能 | 连接 |
|------|------|------|
| PA6  | 加法按键 | 接按键到GND |
| PA7  | 减法按键 | 接按键到GND |

按键默认使用内部上拉电阻，按下时拉低。

## 测试要点

1. **消抖处理**：代码实现了5ms×5的消抖，避免误触发
2. **边界限制**：计数器限制在0-99之间
3. **实时更新**：每次按键立即更新BLE发送内容
4. **调试友好**：串口输出详细的按键和发送信息

## 扩展功能

### 1. **修改显示格式**
在 `set_ble_counter_value()` 函数中修改字符串格式：
```c
// 当前格式
snprintf((char *)counter_data, sizeof(counter_data), "Hera %d", value);

// 可改为
snprintf((char *)counter_data, sizeof(counter_data), "Count:%d", value);
// 或
snprintf((char *)counter_data, sizeof(counter_data), "VAL:%03d", value);
```

### 2. **修改计数范围**
在 `pa6_key_polling_task_handle()` 中修改：
```c
// 当前限制
if (ble_counter > 99) ble_counter = 99;
if (ble_counter < 0) ble_counter = 0;

// 可改为
if (ble_counter > 255) ble_counter = 255;  // 8位最大值
if (ble_counter < -128) ble_counter = -128;  // 支持8位有符号
```

### 3. **添加更多功能**
- 长按功能：检测长按触发特殊操作
- 双击功能：检测快速双击
- 组合键：PA6+PA7同时按下触发复位

## 故障排查

1. **按键无响应**
   - 检查引脚连接
   - 查看串口是否有按键任务启动信息
   - 确认GPIO配置正确

2. **BLE无数据**
   - 确认已启用ae04 notify
   - 检查 `TEST_SEND_DATA_RATE` 宏是否为1
   - 查看串口是否有 "BLE data sent" 日志

3. **数据格式错误**
   - 检查 `counter_data` 数组大小
   - 确认 `snprintf` 返回值正确

## 恢复原始功能
如需恢复原始的Opus编码功能：
1. 取消 `le_rcsp_adv_module.c` 中所有注释
2. 恢复原始的数据打包逻辑
3. 恢复时钟和编码器管理

---
*更新日期: 2025-12-07*