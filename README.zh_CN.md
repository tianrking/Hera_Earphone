# Hera JL7016 耳机固件

这是 Hera 耳机原型的 MCU 固件仓库，基于杰理 AC701N/JL7016 平台。当前分支重点维护
BLE OPUS 音频传输、HFP/eSCO 通话音频抓取，以及 Android 调试 App 的双路音频验证。

<p>
  <img alt="平台" src="https://img.shields.io/badge/平台-Jieli%20AC701N%20%2F%20JL7016-263238?style=for-the-badge">
  <img alt="语言" src="https://img.shields.io/badge/语言-C-00599C?style=for-the-badge&logo=c&logoColor=white">
  <img alt="传输" src="https://img.shields.io/badge/传输-BLE%20GATT-0082FC?style=for-the-badge&logo=bluetooth&logoColor=white">
  <img alt="音频" src="https://img.shields.io/badge/音频-OPUS%2016k-00A67E?style=for-the-badge">
  <img alt="通话" src="https://img.shields.io/badge/通话-HFP%20%2B%20eSCO-6D4C41?style=for-the-badge">
</p>

## 当前音频功能

AE04 BLE notify 当前使用 84 字节包，同时携带两路 OPUS 音频：

```text
byte 0      : VAD 标记
byte 1-40   : 本机 mic / 通话上行侧 OPUS
byte 41-80  : 对方声音 / HFP 下行侧 OPUS
byte 81     : 序号
byte 82-83  : 预留 / 可选长度字段
```

预期行为：

| 场景 | byte 1-40 | byte 41-80 |
| --- | --- | --- |
| 普通 BLE 录音 | 本机 mic OPUS | 通常为空或不使用 |
| 电话通话中 | 本机通话 mic/AEC OPUS | 对方 HFP/eSCO 声音 OPUS |

Android 调试 App 已对齐该包格式，可以通过 `Play Mic 1-40` 和
`Play Peer 41-80` 分别验证前后两路音频。

## 技术实现细节

主要实现文件：

```text
apps/common/third_party_profile/jieli/JL_rcsp/bt_trans_data/le_rcsp_adv_module.c
```

关键路径：

| 函数 | 作用 |
| --- | --- |
| `rec_enc_mic_output()` | 将本机 mic/AEC 编码后的 OPUS 写入 `opus_mic_buffer` |
| `rec_enc_dec_output()` | 将对方/HFP 下行音频编码后的 OPUS 写入 `opus_dec_buffer` |
| `ae04_call_pcm_output()` | 将通话 AEC/上行 PCM 喂给本机 OPUS encoder |
| `ae04_playback_pcm_output()` | 将 HFP/eSCO 播放 PCM 转为 16 kHz mono 后喂给对方音频 encoder |
| `test_data_send_packet()` | 组装 AE04 BLE notify 包并发送 |

当前通话双路开关：

```c
#define AE04_CALL_DUAL_STREAM  1
```

开启后，通话中 AE04 notify 会等待本机和对方两路 OPUS frame 都准备好，再发送一包
`mic+peer` 双路数据。

通话测试时建议关注串口日志：

```text
ae04 opus enter call
ae04 opus call open mic_ret:0 peer_ret:0
ae04 mic opus frame:...
ae04 peer opus frame:...
ae04 notify ok:... src:mic+peer ...
```

## 编译命令

以下命令都在仓库根目录执行，例如 `F:\Hera_Earphone`。

推荐使用项目自带的编译入口：

```powershell
.vscode\winmk.bat all
```

清理编译输出：

```powershell
.vscode\winmk.bat clean
```

该入口会自动将 `tools\utils` 加入 `PATH`，并使用项目自带的
`make.exe`、`mkdir_win.exe`、`fixbat.exe`、`rm.exe` 等工具。

也可以先打开项目准备好的命令行：

```powershell
tools\make_prompt.bat
```

然后在弹出的 `cmd` 窗口里执行：

```cmd
make all
```

如果直接调用杰理工具链里的 `make.exe`，且当前环境找不到 `mkdir_win.exe`，
可以使用下面的兜底命令：

```powershell
C:\JL\mc\bin\make.exe all MKDIR="powershell -NoProfile -Command 'New-Item -ItemType Directory -Force -Path `$args[0] | Out-Null'"
```

生成的固件包通常在：

```text
cpu\br28\tools\download\earphone\update.ufw
```

## 测试流程

1. 执行 `.vscode\winmk.bat all` 编译固件。
2. 烧录 `cpu\br28\tools\download\earphone\update.ufw`。
3. 使用 Android 调试 App 连接耳机 BLE。
4. Subscribe AE04 notify。
5. 非通话场景验证 `Play Mic 1-40`。
6. 电话通话中分别验证：
   - `Play Mic 1-40`：本机通话侧声音。
   - `Play Peer 41-80`：对方通话声音。
7. 串口确认出现 `src:mic+peer` 日志。

## 可追溯记录

本仓库会保留固件 bring-up 到双路通话音频实现的历史提交链路。详细变更记录维护在
[CHANGELOG.md](CHANGELOG.md)，其中按 commit hash 记录了每次增加、修改和修复的内容，
方便后续排查和回溯。
