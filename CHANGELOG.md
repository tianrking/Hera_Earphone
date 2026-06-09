# Changelog

This changelog is maintained from the actual Git history on the current firmware
branch. Commit hashes are kept so firmware behavior can be traced back to the
implementation that introduced it.

## 2026-06-09

### `715dc16` - Send AE04 call mic and peer opus streams

- Added AE04 dual-stream phone-call mode.
- Kept the first 40 OPUS bytes as local mic/AEC audio.
- Routed HFP/eSCO downlink playback audio into the second 40 OPUS bytes.
- Added paired notify behavior for phone calls so BLE sends `mic+peer` packets.
- Added source-aware logs such as `ae04 mic opus frame`, `ae04 peer opus frame`,
  and `ae04 notify ok ... src:mic+peer`.
- Documented build commands in `README.md` and `README.zh_CN.md`.

## 2026-06-06

### `d867ffe` - modify

- Reworked AE04 call audio handling around phone-call verification.
- Added call-side PCM buffering and reference/downlink PCM handling.
- Added first-stage logic for routing playback reference audio during calls.
- Reduced reliance on a standalone reference stream during call start.
- Improved AE04 call-path logging and BLE notify behavior under eSCO load.

## 2026-05-14

### `d4c647e` - Add AE04 playback reference audio stream

- Added a second decoder/encoder path for playback reference audio.
- Introduced `audio_dec_ref_enc_open()` and `audio_dec_ref_enc_close()` support.
- Captured A2DP/eSCO playback PCM through `ae04_playback_pcm_output()`.
- Resampled playback PCM toward the 16 kHz OPUS encoder target.
- Prepared the AE04 packet format for mic plus playback/downlink audio.

### `2b4f6cd` - Support AE04 recording during phone calls

- Added phone-call entry and exit hooks for AE04 OPUS streaming.
- Switched AE04 capture behavior when eSCO/HFP is active.
- Routed call AEC/uplink output into the OPUS encode path.
- Integrated call open/close handling with earphone connection status events.

## 2026-05-09

### `574bde0` - Document BLE OPUS mic streaming setup

- Added documentation for BLE OPUS mic streaming setup and expected workflow.
- Captured early build and verification notes for the AE04 streaming path.

### `ac803f8` - Throttle AE04 mic frame notifications

- Reduced AE04 mic-frame notification pressure.
- Added throttling behavior to avoid overwhelming BLE notify during streaming.
- Improved stability for continuous OPUS frame delivery.

### `fc60736` - Enable BLE OPUS mic streaming

- Enabled the first AE04 BLE OPUS microphone streaming path.
- Added OPUS frame buffering and BLE notify packing.
- Established the original `byte 1-40` mic OPUS payload convention.

## 2025-12-09

### `53823ed` - FIX PA6(ADC) REEC1 -> PA7 for test button

- Moved the test button ADC mapping from PA6/REEC1 to PA7 for hardware testing.
- Adjusted board-level input configuration for the test-button path.

## 2025-12-07

### `4f321cb` - FIX AUDIO_MIC_CAPLESS_MODE & MIC_PWR_FROM_MIC_LDO

- Adjusted microphone analog/power configuration.
- Updated capless microphone mode and microphone LDO power selection.
- Stabilized the microphone capture hardware configuration.

## 2025-12-06

### `0240a33` - Add PA6 button

- Added PA6 button support for board-level input testing.
- Extended early hardware interaction points for the earphone board.

## 2025-12-04

### `0cdcb3c` - FIX ble service

- Fixed BLE service configuration during early bring-up.
- Improved BLE service discoverability and compatibility with the debug app.

## 2025-12-03

### `7d4102a` - change bt name & change makefile

- Updated Bluetooth naming for the Hera firmware target.
- Adjusted Makefile/build configuration for the local project layout.

### `f0506c7` - first commit

- Initial firmware project import.
- Established the baseline Jieli AC701N/JL7016 earphone firmware tree.
