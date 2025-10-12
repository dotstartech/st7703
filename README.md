# GX040HD-30MB-A1 LCD Panel

This repository provides a Linux kernel module and device tree overlays for integrating the ``GX040HD-30MB-A1`` (or similar ``TL040HDS21CT2-B1503A``) 4.0" 720x720 IPS LCD panel with the Raspberry Pi Compute Module 4 (CM4). The ``GX040HD-30MB-A1`` IPS LCD panel features the Sitronix ``ST7703`` LCD controller and the FocalTech ``FT6336U`` touch controller. It enables direct connection of the panel to the Pi’s ``MIPI DSI`` interface, supporting 4-lane operation and full RGB888 color. The LCD panel driver is based on the [upstream Linux kernel module ST7703](https://github.com/raspberrypi/linux/blob/rpi-6.6.y/drivers/gpu/drm/panel/panel-sitronix-st7703.c).

Below are a few sources where the LCD panel can be purchased:
 - https://chance-display.com/product/4-inch-720720-mipi-interface-touch-screen-square-ips-tft-lcd-module
 - https://www.panelook.com/GX040HD-30MRB-A1HDMI-board-4-inch-Square-display-screen-30PINS-MIPI-720-720-IPS-HDMI-board-with-detail_181668.html

The repository includes build and installation instructions, and troubleshooting guidance for seamless deployment on Raspberry Pi OS with kernel version ``6.12.25+rpt-rpi-v8`` or compatible. This project is intended for developers, integrators, and hardware enthusiasts seeking to add high-resolution square LCD support to custom Raspberry Pi-based systems.

**References:**
- [GX040HD Panel Specification](docs/GX040HD-30MB-A1.pdf)
- [ST7703 Datasheet](docs/ST7703_DS_v01_20160128.pdf)
- [Initialization Sequence](docs/ST7703_QV040YNQ-N80_IPS_code_2power_4Lane_V1.0_20250611.txt)
- [FT6336U Datasheet](docs/FT6336U-DataSheet-V1.1.pdf)
- [Raspberry Pi Linux ST7703 Driver](https://github.com/raspberrypi/linux/blob/rpi-6.6.y/drivers/gpu/drm/panel/panel-sitronix-st7703.c)
- [Raspberry Pi Linux edt-ft5x06 Driver](https://github.com/raspberrypi/linux/blob/rpi-6.6.y/drivers/input/touchscreen/edt-ft5x06.c)
- [Raspberry Pi Device Tree Documentation](https://www.raspberrypi.com/documentation/computers/configuration.html#device-trees-overlays-and-parameters)
- [Linux DRM Panel Documentation](https://www.kernel.org/doc/html/latest/gpu/drm-kms-helpers.html#panel-helper-reference)

## Hardware Specifications and Setup

- **Model**: GX040HD-30MB-A1
- **Display Controller**: Sitronix ST7703
- **Size**: 4.0 inch diagonal
- **Resolution**: 720 x 720 pixels
- **Type**: IPS LCD
- **Interface**: MIPI DSI 4-lane
- **Pixel clock**: 41.4 MHz
- **Active area**: 89.6 x 89.6 mm
- **Touch Controller**: FocalTech FT6336U
    - **Interface**: I2C0 - ID_SC/ID_SD (GPIO 0/1), address 0x48
    - **Touch Points**: Up to 2 simultaneous touches
    - **Resolution**: 720 x 720 (matches display)

**Display Timing Parameters** (based on the initialization sequence from vendor):
- **HFP (Horizontal Front Porch)**: 80 pixels
- **HSA (Horizontal Sync Active)**: 20 pixels  
- **HBP (Horizontal Back Porch)**: 80 pixels
- **VFP (Vertical Front Porch)**: 30 lines
- **VSA (Vertical Sync Active)**: 4 lines
- **VBP (Vertical Back Porch)**: 12 lines

**GPIO Connections:**
- **Display Reset**: GPIO 23 (configurable in device tree)
- **I2C Bus**: I2C0 (ID_SC=GPIO 0, ID_SD=GPIO 1) - DSI interface bus
- **Touch INT**: GPIO 25 (interrupt, see touchscreen setup)
- **Touch RST**: GPIO 24 (reset, see touchscreen setup)
- **Power supplies**: VCC: 3.3V (main power) and IOVCC: 3.3V (I/O power)

**DSI Configuration:**
- **Interface**: DSI0 on Raspberry Pi (configurable in device tree)
- **Lanes**: 4 lanes (data lanes 0, 1, 2, 3)
- **Format**: RGB888 (24-bit color)

## Build and Installation

```bash
# Build the kernel module
make all

# Install the kernel module
make install

# Build and install device tree overlay
make dto-all

# Load the module
make load
```

Reboot and check kernel messages
```bash
dmesg | grep -E "(st7703|dsi|panel|vc4)"
```

After successful setup, you should see
```
vc4-drm gpu: bound fe204000.dsi (ops vc4_dsi_ops [vc4])
vc4-drm gpu: bound fe700000.dsi (ops vc4_dsi_ops [vc4])
sitronix,st7703: panel found
vc4-drm gpu: [drm] Initialized vc4 0.0.0 for gpu on minor 1
```

## Configuration

Add to `/boot/firmware/config.txt`:
```
display_auto_detect=0
dtoverlay=vc4-kms-v3d
dtoverlay=st7703-gx040hd

# IMPORTANT: Use i2c_vc (for I2C0/DSI), NOT i2c_arm (for I2C1)!
dtparam=i2c_vc=on
dtoverlay=ft6336u-gx040hd
```

For X11 desktop environments, create `/etc/X11/xorg.conf.d/99-fbdev.conf`:

```xorg
Section "Device"
    Identifier "GX040HD"
    Driver "fbdev"
    Option "fbdev" "/dev/fb0"
    Option "SWCursor" "true"
EndSection

Section "Screen"
    Identifier "main"
    Device "GX040HD"
    Monitor "GX040HD Monitor"
EndSection

Section "Monitor"
    Identifier "GX040HD Monitor"
    Option "DPMS" "false"
EndSection
```

### FT6336U Device Tree Properties

1. **I2C Interface**: Enables I2C0 bus (ID_SC/ID_SD - GPIO 0/1)
2. **GPIO Configuration**: Sets up interrupt and reset pins with pull-ups
3. **Touch Device**: 
   - Compatible with `focaltech,ft6236` driver (supports FT6336U)
   - I2C bus: I2C0 (DSI interface bus)
   - I2C address: 0x48
   - Touch resolution: 720x720
   - Interrupt on GPIO 25 (falling edge)
   - Reset on GPIO 24 (active low)

The overlay uses `compatible = "focaltech,ft6236"` because:
- FT6336U is register-compatible with FT6236
- The `edt-ft5x06` driver explicitly supports FT6236
- FT6336U is not in the kernel's device tree bindings but works with FT6236 driver

### Touch coordinates are inverted or rotated

The overlay includes `touchscreen-inverted-x` and `touchscreen-inverted-y` properties. You may need to adjust these based on your display orientation.

Edit `ft6336u-gx040hd-overlay.dts` and modify:

```dts
touchscreen-inverted-x;     /* Remove if X-axis is correct */
touchscreen-inverted-y;     /* Remove if Y-axis is correct */
touchscreen-swapped-x-y;    /* Add if X and Y are swapped */
```

## Testing and Troubleshooting
```bash
# Check kernel module
modinfo panel-sitronix-st7703

# Monitor kernel messages
dmesg -w | grep -E "(st7703|dsi|panel|vc4)"

# Check if display is detected
modetest -M vc4

# List available displays
fbset -s

# Check DRM/KMS status
sudo dtoverlay -l

# Enable debug output
echo 8 > /sys/module/drm/parameters/debug

# Check display controller status  
cat /sys/kernel/debug/dri/0/state

# Check panel power state
cat /sys/class/drm/card0-DSI-1/status

# Check DSI interface
ls /dev/dri/

# Check framebuffer
ls /dev/fb*
```

### Testing Touch Input

```bash
# Basic test with evtest
sudo apt-get install evtest
sudo evtest
# Select the touch device (usually /dev/input/event0)
# Touch the screen and observe events

# Test with libinput
sudo apt-get install libinput-tools
sudo libinput debug-events

# Calibration (if needed)
sudo apt-get install xinput-calibrator
xinput_calibrator
```
