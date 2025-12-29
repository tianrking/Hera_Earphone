# PE6 供电实验记录

## 实验目的
使用 PE6 引脚为外部 W25Q128 Flash 芯片提供电源

## 实验方法对比

### 方法 1: sdpg_config() 单独使用（失败 ❌）

**实现日期**: 2025-12-27

**代码实现**:
```c
#include "asm/power/power_api.h"

// 在 main() 初始化中调用
sdpg_config(1);  // 开启 PE6 供电
```

**问题**: 编译错误 - 头文件无法找到
```
apps/earphone/app_main.c:20:10: fatal error: 'asm/power/power_api.h' file not found
```

**结论**: ❌ 单独使用不可行（头文件包含问题）

---

### 方法 2: GPIO 基本输出（失败 ❌）

**实现日期**: 2025-12-27

**代码实现**:
```c
gpio_set_direction(PE6_CTRL_PIN, 0);  // 输出模式
gpio_set_die(PE6_CTRL_PIN, 1);        // 使能
gpio_set_pull_up(PE6_CTRL_PIN, 0);    // 无上拉
gpio_set_pull_down(PE6_CTRL_PIN, 0);  // 无下拉
gpio_write(PE6_CTRL_PIN, 1);          // 设置为高电平
```

**问题**: W25Q128 Flash 无法正常工作

**结论**: ❌ 基本输出驱动能力不足

---

### 方法 3: GPIO + 增强输出 HD0+HD（失败 ❌）

**实现日期**: 2025-12-27

**代码实现**:
```c
gpio_set_direction(PE6_CTRL_PIN, 0);      // 输出模式
gpio_set_die(PE6_CTRL_PIN, 1);            // 使能数字输入
gpio_set_pull_up(PE6_CTRL_PIN, 0);        // 无上拉
gpio_set_pull_down(PE6_CTRL_PIN, 0);      // 无下拉
gpio_set_hd0(PE6_CTRL_PIN, 1);            // 使能增强驱动 HD0
gpio_set_hd(PE6_CTRL_PIN, 1);             // 使能增强驱动 HD
gpio_write(PE6_CTRL_PIN, 1);              // 设置为高电平
```

**问题**: 仍然无法驱动 W25Q128 Flash

**结论**: ❌ 即使增强驱动也不够

---

### 方法 4: GPIO 增强输出 + sdpg_config 组合（测试中 🔄）

**实现日期**: 2025-12-27

**代码实现**:
```c
// 外部声明 sdpg_config 函数
extern void sdpg_config(int enable);

// GPIO 增强输出配置
gpio_set_direction(PE6_CTRL_PIN, 0);      // 输出模式
gpio_set_die(PE6_CTRL_PIN, 1);            // 使能数字输入
gpio_set_pull_up(PE6_CTRL_PIN, 0);        // 无上拉
gpio_set_pull_down(PE6_CTRL_PIN, 0);      // 无下拉
gpio_set_hd0(PE6_CTRL_PIN, 1);            // 使能增强驱动 HD0
gpio_set_hd(PE6_CTRL_PIN, 1);             // 使能增强驱动 HD
gpio_write(PE6_CTRL_PIN, 1);              // 设置为高电平

// 同时使能 SD Power Gate
sdpg_config(1);  // 开启 PE6 SD Power Gate
```

**组合说明**:
- GPIO 层面：HD0 + HD 双重增强驱动
- Power 层面：sdpg_config 使能 SD 电源门控
- 两种方式同时生效，提供最大驱动能力

**状态**: 🔄 待测试

---

## 技术背景

### sdpg_config 函数
- **声明位置**: `include_lib/driver/cpu/br28/asm/power/power_api.h:172`
- **函数原型**: `void sdpg_config(int enable);`
- **功能**: SD 卡电源门控（SD Power Gate）控制
- **实现位置**: 预编译库 `cpu.a` 中
- **外部声明**: `extern void sdpg_config(int enable);`

### PE6 引脚
- **端口**: PORTE
- **引脚号**: 6
- **GPIO定义**: `IO_PORTE_06`
- **物理引脚**: 需要查看芯片数据手册

### GPIO 增强驱动模式
根据 `gpio.h` 中的定义：
- `gpio_set_hd0()`: 增强输出驱动能力 HD0
- `gpio_set_hd()`: 增强输出驱动能力 HD
- 两者可以同时使能以获得最大驱动能力

---

## 实验结果总结

| 方法 | 状态 | 说明 |
|------|------|------|
| sdpg_config() 单独使用 | ❌ 失败 | 头文件包含问题 |
| GPIO 基本输出 | ❌ 失败 | 驱动能力不足 |
| GPIO + HD0+HD | ❌ 失败 | 增强驱动仍不够 |
| GPIO + HD + sdpg_config | 🔄 测试中 | 组合模式 |

---

## 下一步计划

1. 🔄 测试 GPIO 增强输出 + sdpg_config 组合模式（进行中）
2. 🔍 如果仍失败，考虑使用外部电源管理芯片或 LDO
3. 📝 比较不同方案的功耗差异

---

## 参考资料
- BR28 芯片数据手册
- GPIO 驱动: `include_lib/driver/cpu/br28/asm/gpio.h`
- Power API: `include_lib/driver/cpu/br28/asm/power/power_api.h`
