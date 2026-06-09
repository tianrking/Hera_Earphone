# Hera JL7016 Earphone Firmware

Firmware for the Hera earphone prototype based on the Jieli AC701N/JL7016
platform. This branch focuses on low-power BLE OPUS audio transport and phone
call audio verification.

<p>
  <img alt="Platform" src="https://img.shields.io/badge/Platform-Jieli%20AC701N%20%2F%20JL7016-263238?style=for-the-badge">
  <img alt="Language" src="https://img.shields.io/badge/Language-C-00599C?style=for-the-badge&logo=c&logoColor=white">
  <img alt="Transport" src="https://img.shields.io/badge/Transport-BLE%20GATT-0082FC?style=for-the-badge&logo=bluetooth&logoColor=white">
  <img alt="Audio" src="https://img.shields.io/badge/Audio-OPUS%2016k-00A67E?style=for-the-badge">
  <img alt="Profile" src="https://img.shields.io/badge/Profile-HFP%20%2B%20eSCO-6D4C41?style=for-the-badge">
</p>

## Current Audio Feature

The AE04 BLE notification stream now carries two OPUS audio frames in one
84-byte packet:

```text
byte 0      : VAD flag
byte 1-40   : local mic / uplink-side OPUS frame
byte 41-80  : peer / HFP downlink-side OPUS frame
byte 81     : sequence number
byte 82-83  : reserved / optional length bytes
```

Expected behavior:

| Mode | Bytes 1-40 | Bytes 41-80 |
| --- | --- | --- |
| Normal BLE audio | Local mic OPUS | Usually empty / not used |
| Phone call | Local call mic/AEC OPUS | Remote caller HFP/eSCO OPUS |

The matching Android debug app can select either `Play Mic 1-40` or
`Play Peer 41-80` to verify both halves of the packet.

## Technical Implementation Notes

The main implementation lives in:

```text
apps/common/third_party_profile/jieli/JL_rcsp/bt_trans_data/le_rcsp_adv_module.c
```

Important paths:

| Path | Purpose |
| --- | --- |
| `rec_enc_mic_output()` | Stores encoded local mic/AEC OPUS frames into `opus_mic_buffer` |
| `rec_enc_dec_output()` | Stores encoded peer/downlink OPUS frames into `opus_dec_buffer` |
| `ae04_call_pcm_output()` | Feeds call AEC/uplink PCM into the local OPUS encoder |
| `ae04_playback_pcm_output()` | Resamples HFP/eSCO playback PCM to 16 kHz mono and feeds the peer encoder |
| `test_data_send_packet()` | Packs AE04 notify payload and sends it through BLE |

Current call mode uses:

```c
#define AE04_CALL_DUAL_STREAM  1
```

When enabled, phone-call AE04 packets wait until both local and peer OPUS frames
are ready before notify, so Android receives a paired 84-byte packet.

Firmware logs to look for during call testing:

```text
ae04 opus enter call
ae04 opus call open mic_ret:0 peer_ret:0
ae04 mic opus frame:...
ae04 peer opus frame:...
ae04 notify ok:... src:mic+peer ...
```

## Build Commands

Run commands from the repository root, for example `F:\Hera_Earphone`.

Recommended project-local build:

```powershell
.vscode\winmk.bat all
```

Clean build outputs:

```powershell
.vscode\winmk.bat clean
```

This uses the tool wrappers shipped in `tools\utils`, including `make.exe`,
`mkdir_win.exe`, `fixbat.exe`, and `rm.exe`.

You can also open a prepared command prompt first:

```powershell
tools\make_prompt.bat
```

Then run this in the opened `cmd` window:

```cmd
make all
```

Fallback build command, useful when calling the Jieli make executable directly
and `mkdir_win.exe` is not in `PATH`:

```powershell
C:\JL\mc\bin\make.exe all MKDIR="powershell -NoProfile -Command 'New-Item -ItemType Directory -Force -Path `$args[0] | Out-Null'"
```

The generated firmware package is usually:

```text
cpu\br28\tools\download\earphone\update.ufw
```

## Test Flow

1. Build the firmware with `.vscode\winmk.bat all`.
2. Flash `cpu\br28\tools\download\earphone\update.ufw`.
3. Connect with the Android debug app.
4. Subscribe to AE04 notify.
5. In normal mode, verify `Play Mic 1-40`.
6. During a phone call, verify both:
   - `Play Mic 1-40` for local call-side audio.
   - `Play Peer 41-80` for remote caller audio.
7. Check serial logs for `src:mic+peer`.

## Traceability

This repository keeps the historical commit chain for firmware bring-up. See
[CHANGELOG.md](CHANGELOG.md) for a maintained, human-readable record of each
change from the initial project import through the current AE04 dual-stream call
audio implementation.
