# W25Q128 Flash Integration Note

## Source

Reference project:

```text
F:\w25q128_hera
```

Imported driver files:

```text
apps/earphone/w25q128.c
apps/earphone/w25q128.h
```

This integration intentionally uses the small standalone software-SPI driver, not the larger `norflash_sfc` device/filesystem stack.

## Board Configuration

Current board config:

```text
apps/earphone/board/br28/board_jl701n_demo_cfg.h
```

Current W25Q128 GPIO mapping is the SD-style 3-wire hardware wiring:

```c
#define W25Q128_3WIRE_ENABLE        1
#define W25Q128_IO_PIN              IO_PORTC_05  // SDCLK pad used as bidirectional DO/DI
#define W25Q128_CS_PIN              IO_PORTC_03  // SDDAT pad used as CS
#define W25Q128_CLK_PIN             IO_PORTC_04  // SDCMD pad used as CLK
#define W25Q128_POWER_PIN           IO_PORTE_06  // SDPG/VDD enhanced output
#define W25Q128_POWER_SENSE_PIN     IO_PORTG_08  // Same SDPG pad kept as input
```

Feature switch:

```c
#define TCFG_W25Q128_ENABLE              ENABLE_THIS_MOUDLE
#define TCFG_W25Q128_BOOT_TEST_ENABLE    DISABLE_THIS_MOUDLE
#define TCFG_W25Q128_COUNTER_TEST_ENABLE ENABLE_THIS_MOUDLE
```

`TCFG_W25Q128_BOOT_TEST_ENABLE` is disabled by default because the test erases and writes flash sectors.
`TCFG_W25Q128_COUNTER_TEST_ENABLE` enables the current 1-second append/readback test.

## Pin Notes

- This board is not the earlier MOSI/MISO 4-wire wiring.
- PC5 is switched dynamically: output while sending command/address/write data, input while reading JEDEC/status/flash bytes.
- SD0 is disabled in the current board config, so PC3/PC4/PC5 are available for W25Q128 bitbang.
- PE6 and PG8 refer to the shared SDPG/VDD pad. The driver drives PE6 high and leaves PG8 as input to avoid fighting the same external node.

## Boot Behavior

Boot path:

```text
apps/earphone/app_main.c
  -> w25q128_boot_probe()
  -> w25q128_init()
  -> read JEDEC ID
```

Default boot behavior powers the external flash, initializes GPIO software SPI and checks JEDEC ID. When the counter test is enabled, a background task appends one incrementing record every second and reads it back for verification.

Expected boot logs:

```text
>>> Initializing W25Q128 Flash...
W25Q128: JEDEC ID - Manufacturer: 0xef, Device: 0x4018
>>> W25Q128 Flash initialized successfully!
>>> W25Q128 counter test task created
>>> W25Q128 counter append test start, sector=0x001000
>>> W25Q128 counter OK: index=0 addr=0x001000 value=1
>>> W25Q128 counter OK: index=1 addr=0x00100c value=2
```

## Current Diagnosis

The current hardware bring-up has not reached the W25Q128 protocol layer yet. The
latest UART logs show that PC3/PC4/PC5 cannot be read back high even after the
driver writes the Port C registers directly:

```text
W25Q128 pins [schematic-map]: CS=0 CLK=0 IO=0
W25Q128 PORTC [schematic-map]: OUT=0x00000008 IN=0x00000000 DIR=0x000001e7 DIE=0x00000038 PU=0x00000000 PD=0x00000000
W25Q128 gpio-self [schematic-map]: CS1=0 CLK1=0 IO1=0 IO0=0 IOin=0
W25Q128: JEDEC ID - Manufacturer: 0xff, Device: 0xffff
```

This means the software can set at least the PC3 output bit, but the actual Port C
input sample remains low. The next debug step is to measure PC3/PC4/PC5 on the
board and confirm whether the lines are externally pulled low, mapped to
different package pins, or still controlled by a lower-level special function.

The counter test uses only the 4 KB sector at `0x001000`. It appends 12-byte records:

```text
magic:  0x57323551
value:  incrementing u32
check:  bitwise inverse of value
```

The sector is erased only when it is empty/corrupt/full, not after every write.

## Build

Driver is added to `Makefile`:

```text
apps/earphone/w25q128.c
```

Build command:

```powershell
cd F:\Hera_Earphone
.vscode\winmk.bat all
```

## Public APIs

```c
int w25q128_init(void);
void w25q128_deinit(void);
int w25q128_read_jedec_id(u8 *manuf_id, u16 *device_id);
u8 w25q128_read_status_reg(u8 reg_num);
int w25q128_read_data(u32 addr, u8 *buf, u32 len);
int w25q128_write_data(u32 addr, const u8 *buf, u32 len);
int w25q128_sector_erase(u32 addr);
int w25q128_block_erase_32kb(u32 addr);
int w25q128_block_erase_64kb(u32 addr);
int w25q128_chip_erase(void);
int w25q128_test(void);
```

## Notes

- The driver is software SPI, conservative and easy to isolate.
- Read/write APIs use 24-bit addresses within the 16 MB W25Q128 address space.
- Writes require erase first when changing programmed bits back to `1`.
- Keep destructive tests behind `TCFG_W25Q128_BOOT_TEST_ENABLE`.
