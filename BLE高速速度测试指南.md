# BLE高速传输速度测试指南

## 测试功能说明

按下PA6按键将启动10秒高速测试，设备将以最大速度发送244字节的数据包（内容为重复的"1234567890"），10秒后自动停止并显示统计结果。

## 操作步骤

### 1. 编译烧录
```bash
make clean
make
# 烧录到设备
```

### 2. 测试流程

#### 连接设备
1. 打开手机BLE调试APP（如nRF Connect）
2. 扫描并连接设备
3. 找到ae04 characteristic
4. 启用ae04的notify

#### 开始测试
- **按下PA6**：启动10秒高速测试
  - 设备开始以最大速度发送数据
  - 10秒后自动停止
  - 显示完整的测试结果

### 3. 测试输出

#### 设备串口输出
```
=== Starting 10-Second Max Speed Test ===
Packet size: 244 bytes
Test duration: 10 seconds
Starting now...

=== 10-Second Speed Test Complete ===
Total bytes: 488000
Time: 10.00 seconds
Average speed: 47.66 KB/s (388571 bps)
Packets per second: 2000
=======================================
```

## 上位机测试方案

### 1. Python测试脚本

```python
from bluepy.btle import Peripheral, UUID
import time
import threading

class BLESpeedTest:
    def __init__(self, mac_addr):
        self.peripheral = Peripheral(mac_addr)
        self.byte_count = 0
        self.start_time = None
        self.running = False

    def notification_handler(self, cHandle, data):
        if self.running:
            self.byte_count += len(data)
            if self.byte_count % 24400 == 0:  # 每100包打印一次
                elapsed = time.time() - self.start_time
                speed_kbps = (self.byte_count / elapsed) / 1024
                print(f"Received: {self.byte_count} bytes, "
                      f"Speed: {speed_kbps:.1f} KB/s")

    def start_test(self):
        # 找到ae04 characteristic（需要替换实际的UUID和handle）
        ae04_handle = 0x008e  # 根据实际设备调整

        # 启用notify
        self.peripheral.writeCharacteristic(ae04_handle + 1, b"\x01\x00")

        # 设置通知回调
        self.peripheral.withDelegate(self)

        # 开始计时
        self.running = True
        self.start_time = time.time()
        self.byte_count = 0

        print("Speed test started, waiting for data...")

        # 运行1分钟
        time.sleep(60)

        # 统计结果
        total_time = time.time() - self.start_time
        total_speed_kbps = (self.byte_count / total_time) / 1024
        total_speed_mbps = total_speed_kbps / 1024

        print("\n=== Test Results ===")
        print(f"Total bytes: {self.byte_count:,}")
        print(f"Total time: {total_time:.1f} seconds")
        print(f"Average speed: {total_speed_kbps:.1f} KB/s")
        print(f"Average speed: {total_speed_mbps:.3f} Mbps")

        self.running = False
        self.peripheral.disconnect()

# 使用示例
if __name__ == "__main__":
    # 替换为实际设备MAC地址
    tester = BLESpeedTest("XX:XX:XX:XX:XX:XX")
    tester.start_test()
```

### 2. 数据验证

接收到的数据应该是：
```
12345678901234567890...（连续244字节）
```

## 理论速度计算

### 传输参数
- **连接间隔**: 7.5ms（最小）
- **包大小**: 244字节
- **理论最大频率**: 133包/秒
- **理论最大速率**: 244 × 133 = 32,452字节/秒 ≈ 259.6 kbps

### 实际预期速度
考虑协议开销和处理延迟：
- **预期速率**: 40-50 KB/s
- **包丢失率**: < 0.1%
- **延迟**: 10-20ms

## 性能优化建议

### 1. 手机端优化
- 确保手机蓝牙信号强度良好
- 关闭其他蓝牙设备减少干扰
- 使用支持BLE 5.0的手机

### 2. 环境优化
- 减少WiFi和2.4GHz干扰
- 设备与手机距离保持在1-3米内
- 避免金属障碍物

## 故障排查

### 1. 接收不到数据
- 检查ae04 notify是否启用
- 确认设备显示"Speed test mode ENABLED"
- 检查串口是否有发送统计

### 2. 速度低于预期
- 检查连接间隔是否为7.5ms
- 确认MTU协商为247字节
- 检查是否有包重传

### 3. 数据错误
- 验证接收数据是否为连续的"1234567890"
- 检查是否有数据包丢失

## 预期测试结果

### 理想情况
- **速率**: 48-50 KB/s
- **1分钟接收量**: 约2.8-3.0 MB
- **包丢失**: 0%
- **数据完整性**: 100%

### 可接受范围
- **速率**: 30-50 KB/s
- **1分钟接收量**: 1.8-3.0 MB
- **包丢失**: < 0.1%
- **数据完整性**: > 99.9%

通过这个测试，你可以了解BLE在实际环境中的最大传输能力。