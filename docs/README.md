# ST7703 Initialization Sequence - GX040HD Display

This document explains the ST7703 controller initialization sequence for the GX040HD-30MB-A1 4.0" 720x720 IPS LCD panel and how it maps to the Linux kernel driver implementation.

---

## Display Timing Parameters

```
Resolution: 720 x 720 pixels
Clock: 41.4 MHz

Horizontal Timing:
  - Active pixels (Width):  720
  - Front Porch (HFP):      80
  - Sync Width (HSA):       20
  - Back Porch (HBP):       80
  - Total:                  900 pixels

Vertical Timing:
  - Active lines (Height):  720
  - Front Porch (VFP):      30
  - Sync Width (VSA):       4
  - Back Porch (VBP):       12
  - Total:                  766 lines
```

---

## Initialization Sequence Overview

The initialization consists of sending DSI commands to configure various aspects of the display controller:

1. **SETEXTC** (0xB9): Unlock extended command set
2. **SETMIPI** (0xBA): Configure MIPI DSI interface
3. **SETPOWER_EXT** (0xB8): Extended power control
4. **UNKNOWN_BF** (0xBF): Proprietary configuration
5. **SETRGBIF** (0xB3): RGB interface timing
6. **SETSCR** (0xC0): Source control register
7. **SETVDC** (0xBC): VDC voltage setting
8. **SETPANEL** (0xCC): Panel direction control
9. **SETCYC** (0xB4): Panel inversion
10. **SETDISP** (0xB2): Display resolution
11. **SETEQ** (0xE3): EQ timing control
12. **SETPOWER** (0xC1): Main power control
13. **SETBGP** (0xB5): BGP voltage
14. **SETVCOM** (0xB6): VCOM voltage
15. **SETGIP1** (0xE9): Gate In Panel timing 1
16. **SETGIP2** (0xEA): Gate In Panel timing 2
17. **SETGAMMA** (0xE0): Gamma correction
18. **Exit Sleep** (0x11): Wake up display
19. **Display On** (0x29): Turn on display

---

## Detailed Command Breakdown

### 1. SETEXTC (0xB9) - Unlock Extended Commands
**Purpose:** Enable access to manufacturer-specific commands

```c
// Vendor initialization format:
DSI_CMD(0x04, 0xB9);  // Command length: 4 bytes, Command: 0xB9
DSI_PA(0xF1);         // Parameter 1
DSI_PA(0x12);         // Parameter 2
DSI_PA(0x83);         // Parameter 3

// Kernel driver equivalent:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETEXTC, 0xF1, 0x12, 0x83);
```

**Explanation:**
- This is the "magic sequence" that unlocks ST7703's extended command set
- Required before any other vendor-specific commands
- Values `0xF1, 0x12, 0x83` are the unlock key

---

### 2. SETMIPI (0xBA) - MIPI DSI Configuration
**Purpose:** Configure MIPI DSI interface parameters

```c
// Vendor format:
DSI_CMD(0x1C, 0xBA);  // 28 bytes total (0x1C = 28)
DSI_PA(0x33);         // Lane configuration: 0x33 = 4 lanes
DSI_PA(0x81);         // DSI_LDO_SEL = 1.7V, RTERM = 90 Ohm
DSI_PA(0x05);         // IHSRX = x6 (Low High Speed driving ability)
DSI_PA(0xF9);         // TX_CLK_SEL = fDSICLK/16
DSI_PA(0x0E);         // HFP_OSC (min HFP in DSI mode)
DSI_PA(0x0E);         // HBP_OSC (min HBP in DSI mode)
DSI_PA(0x20);         // Additional parameters (vendor specific)
DSI_PA(0x00); ... DSI_PA(0x37);  // Remaining 20 bytes

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETMIPI,
    0x33,  /* 4 lanes */
    0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00,
    0x90, 0x0A, 0x00, 0x00, 0x01, 0x4F, 0x01, 0x00,
    0x00, 0x37);
```

**Key Parameters:**
- **Byte 1 (0x33):** Lane configuration
  - 0x31 = 2 lanes
  - 0x32 = 3 lanes
  - 0x33 = 4 lanes (used for this panel)
- **Byte 2 (0x81):** Power and termination settings
- **Byte 3 (0x05):** High-speed receiver configuration
- **Byte 4 (0xF9):** Clock selection
- **Bytes 5-6 (0x0E):** Minimum horizontal blanking intervals

---

### 3. SETPOWER_EXT (0xB8) - Extended Power Control
**Purpose:** Configure power IC mode

```c
// Vendor format:
DSI_CMD(0x05, 0xB8);
DSI_PA(0x25);  // 0x75 for 3-power mode, 0x25 for Power IC mode
DSI_PA(0x22);
DSI_PA(0xF0);
DSI_PA(0x63);

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETPOWER_EXT,
    0x25, 0x22, 0xF0, 0x63);
```

**Note:** First parameter selects power mode:
- 0x75 = 3 Power Mode
- 0x25 = Power IC Mode (used here)

---

### 4. UNKNOWN_BF (0xBF) - Proprietary Configuration
**Purpose:** PCR (Power Control Register) setting

```c
// Vendor format:
DSI_CMD(0x04, 0xBF);
DSI_PA(0x02);
DSI_PA(0x11);
DSI_PA(0x00);

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_UNKNOWN_BF,
    0x02, 0x11, 0x00);
```

---

### 5. SETRGBIF (0xB3) - RGB Interface Configuration
**Purpose:** Set RGB interface timing parameters

```c
// Vendor format:
DSI_CMD(0x0B, 0xB3);  // 11 bytes
DSI_PA(0x10);  // VBP_RGB_GEN
DSI_PA(0x10);  // VFP_RGB_GEN
DSI_PA(0x28);  // DE_BP_RGB_GEN (40 decimal)
DSI_PA(0x28);  // DE_FP_RGB_GEN (40 decimal)
DSI_PA(0x03);
DSI_PA(0xFF);
DSI_PA(0x00); DSI_PA(0x00); DSI_PA(0x00); DSI_PA(0x00);

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETRGBIF,
    0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00);
```

---

### 6. SETSCR (0xC0) - Source Control Register
**Purpose:** Configure source driver settings

```c
// Vendor format:
DSI_CMD(0x0A, 0xC0);
DSI_PA(0x73); DSI_PA(0x73); DSI_PA(0x50); DSI_PA(0x50);
DSI_PA(0x00); DSI_PA(0x00); DSI_PA(0x12); DSI_PA(0x70);
DSI_PA(0x00);

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETSCR,
    0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x70, 0x00);
```

---

### 7. SETVDC (0xBC) - VDC Voltage Setting
**Purpose:** Set VDC voltage level

```c
// Vendor format:
DSI_CMD(0x02, 0xBC);
DSI_PA(0x46);  // Default: 0x46

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETVDC, 0x46);
```

---

### 8. SETPANEL (0xCC) - Panel Direction Control
**Purpose:** Set scan direction

```c
// Vendor format:
DSI_CMD(0x02, 0xCC);
DSI_PA(0x0B);  // Forward: 0x0B, Backward: 0x07

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETPANEL, 0x0B);
```

**Note:** Controls scan direction
- 0x0B = Forward scan
- 0x07 = Backward scan

---

### 9. SETCYC (0xB4) - Panel Inversion
**Purpose:** Set column inversion mode

```c
// Vendor format:
DSI_CMD(0x02, 0xB4);
DSI_PA(0x80);

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETCYC, 0x80);
```

---

### 10. SETDISP (0xB2) - Display Resolution and Timing
**Purpose:** Set resolution (RSO - Resolution Setting Option)

```c
// Vendor format:
DSI_CMD(0x04, 0xB2);
DSI_PA(0x3C);  // Resolution setting (720x672)
DSI_PA(0x12);
DSI_PA(0x30);

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETDISP,
    0x3C, 0x12, 0x30);
```

---

### 11. SETEQ (0xE3) - EQ Timing Control
**Purpose:** Configure equalizer timing

```c
// Vendor format:
DSI_CMD(0x0F, 0xE3);  // 15 bytes
DSI_PA(0x07);  // PNOEQ
DSI_PA(0x07);  // NNOEQ
DSI_PA(0x0B);  // PEQGND
DSI_PA(0x0B);  // NEQGND
DSI_PA(0x03);  // PEQVCI
DSI_PA(0x0B);  // NEQVCI
DSI_PA(0x00);  // PEQVCI1
DSI_PA(0x00);  // NEQVCI1
DSI_PA(0x00);  // VCOM_PULLGND_OFF
DSI_PA(0x00);  // VCOM_PULLGND_OFF
DSI_PA(0xFF);  // VCOM_IDLE_ON
DSI_PA(0x00);  // EACH_OPON=0
DSI_PA(0xC0);  // ESD detect function (default: 0xC0)
DSI_PA(0x10);  // SLPOTP

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETEQ,
    0x07, 0x07, 0x0B, 0x0B, 0x03, 0x0B, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0xC0, 0x10);
```

---

### 12. SETPOWER (0xC1) - Main Power Control
**Purpose:** Configure voltage levels for various power rails

```c
// Vendor format:
DSI_CMD(0x0D, 0xC1);  // 13 bytes
DSI_PA(0x36);  // VBTHS VBTLS
DSI_PA(0x00);  // E3
DSI_PA(0x32);  // VSPR (positive supply)
DSI_PA(0x32);  // VSNR (negative supply)
DSI_PA(0x77);  // VSP VSN
DSI_PA(0xF1);  // APS
DSI_PA(0xCC);  // VGH1 VGL1
DSI_PA(0xCC);  // VGH1 VGL1
DSI_PA(0x77);  // VGH2 VGL2
DSI_PA(0x77);  // VGH2 VGL2
DSI_PA(0x33);  // VGH3 VGL3
DSI_PA(0x33);  // VGH3 VGL3

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETPOWER,
    0x36, 0x00, 0x32, 0x32, 0x77, 0xF1, 0xCC, 0xCC,
    0x77, 0x77, 0x33, 0x33);
```

---

### 13. SETBGP (0xB5) - BGP Voltage Setting
**Purpose:** Set band-gap reference voltage

```c
// Vendor format:
DSI_CMD(0x03, 0xB5);
DSI_PA(0x0A);  // vref
DSI_PA(0x0A);  // nvref

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETBGP,
    0x0A, 0x0A);
```

---

### 14. SETVCOM (0xB6) - VCOM Voltage
**Purpose:** Set VCOM (common voltage) for LCD driving

```c
// Vendor format:
DSI_CMD(0x03, 0xB6);
DSI_PA(0xB2);  // Forward VCOM
DSI_PA(0xB2);  // Backward VCOM

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETVCOM,
    0xB2, 0xB2);
```

**Note:** VCOM affects contrast and flicker. Both forward and backward are set to 0xB2.

---

### 15. SETGIP1 (0xE9) - Gate In Panel Control 1
**Purpose:** Configure gate timing and signal routing (64 bytes)

This is a complex command that configures the Gate In Panel (GIP) circuit, which controls the scanning sequence of the LCD rows.

```c
// Vendor format (abbreviated):
DSI_CMD(0x40, 0xE9);  // 64 bytes
DSI_PA(0xC8);  // PANSEL (panel selection)
DSI_PA(0x10);  // SHR_0[11:8] (shift register timing)
DSI_PA(0x0A);  // SHR_0[7:0]
// ... (60 more bytes)

// Kernel driver (abbreviated):
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETGIP1,
    0xC8, 0x10, 0x0A, 0x10, 0x0F, 0xA1, 0x80, 0x12,
    0x31, 0x23, 0x47, 0x86, 0xA1, 0x80, 0x47, 0x08,
    0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0D, 0x00, 0x00, 0x00, 0x48, 0x02, 0x8B, 0xAF,
    0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x48,
    0x13, 0x8B, 0xAF, 0x57, 0x13, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
```

**Key Configuration Areas:**
- **Bytes 1-5:** Panel selection and shift register timing
- **Bytes 6-15:** Gate signal timing (SHP, SCP, CHR, CON, COFF, CHP, CCP)
- **Bytes 16-28:** Gate signal timing control (CGTS_L, CGTS_INV_L)
- **Bytes 29-50:** COS mapping for left side (signal routing)
- **Bytes 51-end:** Additional timing options

---

### 16. SETGIP2 (0xEA) - Gate In Panel Control 2
**Purpose:** Additional gate timing configuration (62 bytes)

```c
// Vendor format (abbreviated):
DSI_CMD(0x3E, 0xEA);  // 62 bytes
DSI_PA(0x96);  // ys2_sel[1:0]
DSI_PA(0x12);  // user_gip_gate1[7:0]
// ... (59 more bytes)

// Kernel driver (abbreviated):
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETGIP2,
    0x96, 0x12, 0x01, 0x01, 0x01, 0x78, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x4F, 0x31, 0x8B, 0xA8,
    0x31, 0x75, 0x88, 0x88, 0x88, 0x88, 0x88, 0x4F,
    0x20, 0x8B, 0xA8, 0x20, 0x64, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x23, 0x00, 0x00, 0x01, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xA1,
    0x80, 0x00, 0x00, 0x00, 0x00);
```

**Configuration Areas:**
- **Bytes 1-12:** Additional gate timing parameters
- **Bytes 13-50:** COS mapping for right side
- **Bytes 51-end:** EQ options, delay settings, clock configurations

---

### 17. SETGAMMA (0xE0) - Gamma Correction
**Purpose:** Set gamma correction curves for optimal image quality

The gamma curve controls the brightness response across different gray levels. The same curve is applied to both positive and negative polarities.

```c
// Vendor format:
DSI_CMD(0x23, 0xE0);  // 35 bytes (17 for positive + 17 for negative + command)
// Positive gamma (17 values):
DSI_PA(0x00); DSI_PA(0x0A); DSI_PA(0x0F); DSI_PA(0x29);
DSI_PA(0x3B); DSI_PA(0x3F); DSI_PA(0x42); DSI_PA(0x39);
DSI_PA(0x06); DSI_PA(0x0D); DSI_PA(0x10); DSI_PA(0x13);
DSI_PA(0x15); DSI_PA(0x14); DSI_PA(0x15); DSI_PA(0x10);
DSI_PA(0x17);
// Negative gamma (17 values - same as positive in this case):
DSI_PA(0x00); DSI_PA(0x0A); DSI_PA(0x0F); DSI_PA(0x29);
DSI_PA(0x3B); DSI_PA(0x3F); DSI_PA(0x42); DSI_PA(0x39);
DSI_PA(0x06); DSI_PA(0x0D); DSI_PA(0x10); DSI_PA(0x13);
DSI_PA(0x15); DSI_PA(0x14); DSI_PA(0x15); DSI_PA(0x10);
DSI_PA(0x17);

// Kernel driver:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETGAMMA,
    0x00, 0x0A, 0x0F, 0x29, 0x3B, 0x3F, 0x42, 0x39,
    0x06, 0x0D, 0x10, 0x13, 0x15, 0x14, 0x15, 0x10,
    0x17, 0x00, 0x0A, 0x0F, 0x29, 0x3B, 0x3F, 0x42,
    0x39, 0x06, 0x0D, 0x10, 0x13, 0x15, 0x14, 0x15,
    0x10, 0x17);
```

**Gamma Curve Structure:**
- Values represent brightness levels from black (0x00) to white (0x3F)
- First 17 bytes: Positive polarity gamma
- Last 17 bytes: Negative polarity gamma
- Adjusting these values affects contrast, brightness, and color accuracy

---

### 18. Exit Sleep Mode (0x11)
**Purpose:** Wake the display from sleep mode

```c
// Vendor format:
DSI_CMD(0x01, 0x11);  // Sleep Out
DelayX1ms(250);        // Wait 250ms

// Kernel driver:
// This is handled in st7703_enable() function, not in init_sequence
mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
mipi_dsi_msleep(&dsi_ctx, 250);
```

**Note:** 250ms delay is required after exit sleep before display can be turned on.

---

### 19. Display On (0x29)
**Purpose:** Turn on the display output

```c
// Vendor format:
DSI_CMD(0x01, 0x29);  // Display On
DelayX1ms(50);         // Wait 50ms

// Kernel driver:
// This is handled in st7703_enable() function, not in init_sequence
mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
mipi_dsi_msleep(&dsi_ctx, 50);
```

---

## Understanding the Kernel Driver Structure

### Command Format Translation

**Vendor Format:**
```c
DSI_CMD(length, command);  // Specify length and command byte
DSI_PA(parameter);          // Add each parameter
```

**Kernel Driver Format:**
```c
mipi_dsi_dcs_write_seq_multi(dsi_ctx, command, param1, param2, ...);
// Length is calculated automatically
```

### Function Flow

```
1. st7703_probe()
   └─> Panel registered with DRM subsystem

2. st7703_prepare()
   ├─> Power on regulators (VCC, IOVCC)
   ├─> Reset display (via GPIO)
   └─> Call init_sequence() -> gx040hd_init_sequence()
       └─> Send all initialization commands

3. st7703_enable()
   ├─> Exit sleep mode (0x11)
   └─> Display on (0x29)

4. st7703_disable()
   └─> Display off

5. st7703_unprepare()
   ├─> Enter sleep mode
   ├─> Reset display
   └─> Power off regulators
```

---

## Key Differences from Vendor Code

### In the Kernel Driver:

1. **Sleep Out and Display On are separate:**
   - Not included in `gx040hd_init_sequence()`
   - Handled in `st7703_enable()` function
   - This follows DRM panel driver conventions

2. **Error handling:**
   - Uses `mipi_dsi_multi_context` for automatic error accumulation
   - All commands can be chained, errors are checked at the end

3. **Standard DCS commands:**
   - Sleep (0x11) and Display On (0x29) use DRM's standard DCS helpers
   - Vendor commands use generic write functions

4. **Timing is embedded:**
   - Delays are built into the sequence using `mipi_dsi_msleep()`
   - No separate delay functions needed

---

## Modifying the Initialization Sequence

### To adjust a parameter:

1. **Find the command** in the vendor sequence above
2. **Locate the corresponding line** in `panel-sitronix-st7703-gx040hd.c`
3. **Modify the parameter values** in the kernel driver
4. **Rebuild and reload** the kernel module

### Example: Changing VCOM voltage

```c
// Original:
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETVCOM, 0xB2, 0xB2);

// To make display darker (reduce VCOM):
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETVCOM, 0xA0, 0xA0);

// To make display brighter (increase VCOM):
mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETVCOM, 0xC0, 0xC0);
```

### Example: Adjusting gamma

Modify the 34 gamma values in the `ST7703_CMD_SETGAMMA` command to adjust brightness curve.

---

## Common Issues and Troubleshooting

| Issue | Likely Cause | Command to Check |
|-------|--------------|------------------|
| Display doesn't turn on | Power sequence or sleep timing | SETPOWER, Sleep timing |
| Flickering | VCOM voltage | SETVCOM (0xB6) |
| Wrong colors | RGB interface | SETRGBIF (0xB3) |
| Dim display | Gamma or power | SETGAMMA (0xE0), SETPOWER (0xC1) |
| Horizontal lines | GIP timing | SETGIP1 (0xE9), SETGIP2 (0xEA) |
| No communication | MIPI config or SETEXTC | SETEXTC (0xB9), SETMIPI (0xBA) |
