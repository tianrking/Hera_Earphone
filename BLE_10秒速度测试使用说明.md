# BLE Flash 10秒速度测试使用说明

## 功能概述
按下PA6按键即可启动10秒BLE速度测试，自动以最大速度发送数据包，并显示实时速度和最大速度。

## 使用步骤

### 1. 切换到Opus模式
```
at+opusmode=1
```

### 2. 确保BLE连接
- 使用APP或BLE调试工具连接设备
- 设备名称：br22_ble_test

### 3. 开始测试
- 按下PA6按键
- 系统自动开始10秒速度测试
- 每500ms显示一次当前速度、最大速度和平均速度

## 测试参数
- **测试时长**: 10秒
- **数据包大小**: 244字节（MTU最大值）
- **发送间隔**: 5ms（尽可能快）
- **连接参数**: 自动请求7.5ms最小间隔

## 输出示例
```
====================================
PA6 Pressed! Starting speed test...
====================================
===== Starting 10-second speed test =====
Sending 244-byte packets at maximum speed...
Speed test mode ENABLED - sending 244-byte packets at max speed for 10 seconds

[9s remaining] Current: 45.2 KB/s | Max: 46.1 KB/s | Avg: 44.8 KB/s | Sent: 402.1 KB
[8s remaining] Current: 46.5 KB/s | Max: 46.5 KB/s | Avg: 45.2 KB/s | Sent: 451.3 KB
...
[0s remaining] Current: 45.8 KB/s | Max: 47.2 KB/s | Avg: 45.5 KB/s | Sent: 921.5 KB

====================================
  10-Second Speed Test Complete!
====================================
Total bytes: 94208 (92.00 KB)
Time: 10.00 seconds
Average speed: 9.20 KB/s (9420 bps)
MAX speed: 9.45 KB/s (9676 bps)
Packets sent: 386
Packets per second: 38
====================================
```

## 注意事项
1. 必须先进入Opus模式才能进行速度测试
2. 确保BLE已连接，否则无法发送数据
3. 测试会自动在10秒后停止
4. 测试期间会持续发送244字节的数据包

## 技术细节
- 使用ATT_CHARACTERISTIC_ae04_01_VALUE_HANDLE发送数据
- 自动优化BLE连接参数以获得最大吞吐量
- 实时计算瞬时速度（500ms窗口）和平均速度