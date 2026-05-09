# BLE OPUS MIC Stream Note

## Current Board

The active board selector is:

```c
#define CONFIG_BOARD_JL701N_DEMO
```

Main files:

- `apps/earphone/board/br28/board_config.h`
- `apps/earphone/board/br28/board_jl701n_demo_cfg.h`
- `apps/common/third_party_profile/jieli/JL_rcsp/bt_trans_data/le_rcsp_adv_module.c`
- `cpu/br28/fft_and_pca.h`

## Working Changes

### MIC Hardware Mode

Hardware is differential MIC with DC blocking capacitors, so the board config now uses differential mode even when ANC/hearing-aid features are disabled.

File:

```text
apps/earphone/board/br28/board_jl701n_demo_cfg.h
```

Effective mode:

```c
#define TCFG_AUDIO_MIC_MODE     AUDIO_MIC_CAP_DIFF_MODE
#define TCFG_AUDIO_MIC1_MODE    AUDIO_MIC_CAP_DIFF_MODE
#define TCFG_AUDIO_MIC2_MODE    AUDIO_MIC_CAP_DIFF_MODE
#define TCFG_AUDIO_MIC3_MODE    AUDIO_MIC_CAP_DIFF_MODE
```

### OPUS Encoder

AE04 real audio streaming requires OPUS encoder support.

File:

```text
apps/earphone/board/br28/board_jl701n_demo_cfg.h
```

Current setting:

```c
#define TCFG_ENC_OPUS_ENABLE    ENABLE
```

If this is disabled, BLE AE04 packets can still arrive on Android, but the MIC OPUS buffer may stay all zero because `audio_mic_enc_open(... AUDIO_CODING_OPUS ...)` cannot produce frames.

### AE04 Debug Pattern

AE04 test pattern is disabled.

File:

```text
apps/common/third_party_profile/jieli/JL_rcsp/bt_trans_data/le_rcsp_adv_module.c
```

Current setting:

```c
#define AE04_SEND_DEBUG_PATTERN 0
```

When enabled, firmware sends a changing `"1122334455"` debug payload instead of real MIC OPUS data.

### AE04 Send Throttle

AE04 now sends only when a new MIC OPUS frame is available. After a successful notify, the frame is marked as sent. This avoids repeatedly notifying old audio frames and causing the phone playback queue to build up several seconds of delay.

Relevant flow:

```text
rec_enc_mic_output()
  -> opus_mic_buffer
  -> opus_mic_buffer_sent = false

test_data_send_packet()
  -> return if opus_mic_buffer_sent == true
  -> send AE04 notify
  -> opus_mic_buffer_sent = true on success
```

## AE04 Packet Format

Current full packet length:

```text
84 bytes
```

Definitions:

```text
cpu/br28/fft_and_pca.h
OPUS_PART_BYTE    = 40
OPUS_PACKAGE_BYTE = 84
DEBUG_BYTE        = 3
```

Current layout:

```text
byte 0      : vad_is_activate
byte 1-40   : MIC OPUS payload
byte 41-80  : DEC OPUS payload / reference payload
byte 81     : send_index
byte 82-83  : reserved/debug
```

Android should decode only:

```text
packet[1..40]
```

Do not feed VAD, DEC, or debug bytes into the OPUS decoder.

## Android Notes

Android project:

```text
C:\Users\Administrator\AndroidStudioProjects\hera_app_dev
```

Fixed behavior:

- Decode only the MIC OPUS frame from AE04.
- Do not fallback to the DEC payload for playback.
- Keep a low-latency decode queue and drop old frames when needed.

The previous queue size was 500 frames. With an OPUS frame around 20 ms, that can accumulate roughly 10 seconds of audio, which matched the observed 8 second delay.

## Build And Flash

Current project root:

```powershell
cd F:\Hera_Earphone
```

Build and flash:

```powershell
.vscode\winmk.bat all
```

Clean:

```powershell
.vscode\winmk.bat clean
```

VSCode route:

```text
Ctrl+Shift+B -> all
```

or:

```text
TASK EXPLORER > SDK > vscode > all
```

Board flashing checklist:

1. Connect the forced-download tool to the board and PC.
2. Press the forced-download button so the board enters download mode.
3. Run `.vscode\winmk.bat all`.
4. Wait for the firmware download to complete.
5. Press the button again to run the new firmware.

## Git Checkpoints

Firmware commits:

```text
fc60736 Enable BLE OPUS mic streaming
ac803f8 Throttle AE04 mic frame notifications
```

Android commits:

```text
69ba291 Mark Android BLE audio sync point
3af960d Reduce AE04 audio playback latency
```

Local backup before Android latency changes:

```text
F:\ProjectBackups\20260509_214451
```

## Next Validation

Recommended iOS BLE checks:

1. Discover AE00 service and AE04 characteristic.
2. Enable AE04 notify.
3. Confirm packet length is normally 84 bytes.
4. Decode only bytes 1-40 as MIC OPUS.
5. Track notify rate, sequence loss, decode errors, and playback delay.

