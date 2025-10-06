# ST7703 Driver for GX040HD-30MB-A1 LCD Panel

This repository provides a Linux kernel module and device tree overlay for integrating the ``GX040HD-30MB-A1`` 4.0" 720x720 IPS LCD panel, featuring the Sitronix ``ST7703`` controller, with the Raspberry Pi Compute Module 4 (CM4). It enables direct connection of the panel to the Pi’s ``MIPI DSI`` interface, supporting 4-lane operation and full RGB888 color. The driver implements the complete initialization and configuration sequence from the vendor, including power management, timing parameters, and gamma correction, ensuring reliable operation and optimal display quality.

The repository includes build scripts, installation instructions, and troubleshooting guidance for seamless deployment on Raspberry Pi OS with kernel version ``6.12.25.25+rpt-rpi-v8`` or compatible. This project is intended for developers, integrators, and hardware enthusiasts seeking to add high-resolution square LCD support to custom Raspberry Pi-based systems.

## Panel Specifications

- **Model**: GX040HD-30MB-A1
- **Controller**: Sitronix ST7703
- **Size**: 4.0 inch diagonal
- **Resolution**: 720 x 720 pixels
- **Type**: IPS LCD
- **Interface**: MIPI DSI 4-lane
- **Pixel clock**: 41.4 MHz
- **Active area**: 89.6 x 89.6 mm

## Display Timing Parameters

Based on the initialization sequence from vendor documentation:
- **HFP (Horizontal Front Porch)**: 80 pixels
- **HSA (Horizontal Sync Active)**: 20 pixels  
- **HBP (Horizontal Back Porch)**: 80 pixels
- **VFP (Vertical Front Porch)**: 30 lines
- **VSA (Vertical Sync Active)**: 4 lines
- **VBP (Vertical Back Porch)**: 12 lines

## Hardware Setup

### GPIO Connections
- **Reset GPIO**: GPIO 23 (configurable in device tree)
- **Power supplies**: VCC: 3.3V (main power) and IOVCC: 3.3V (I/O power)

### DSI Configuration
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
make overlay

# Load the module
make load
```

```bash
# Compile overlay
sudo dtc -@ -I dts -O dtb -o st7703-gx040hd.dtbo st7703-gx040hd-overlay.dts
sudo cp st7703-gx040hd.dtbo /boot/firmware/overlays/

# Test with new overlay
sudo dtoverlay -r st7703-gx040hd
sudo dtoverlay st7703-gx040hd
```

### Reboot and Check
```bash
sudo reboot
dmesg | grep -i st7703
dmesg | grep -i dsi
dmesg | grep -i panel
```

After successful setup, you should see:
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
```

To load overlay manually run
```bash
sudo dtoverlay st7703-gx040hd
```

## Verification

Check if the module loaded correctly
```bash
dmesg | grep -i st7703
modinfo panel-sitronix-st7703
```

Verify display detection
```bash
# Check if display is detected
modetest -M vc4

# List available displays
fbset -s

# Check DRM/KMS status
sudo dtoverlay -l
```

## Troubleshooting

```bash
# Enable debug output
echo 8 > /sys/module/drm/parameters/debug

# Check display controller status  
cat /sys/kernel/debug/dri/0/state

# Monitor DSI interface
dmesg | grep -i dsi

# Check panel power state
cat /sys/class/drm/card0-DSI-1/status

# Check DSI interface
ls /dev/dri/

# Check framebuffer
ls /dev/fb*

# Monitor kernel messages
dmesg -w | grep -E "(st7703|dsi|panel|vc4)"
```

## License

This driver is based on the upstream Linux kernel ST7703 panel driver and is licensed under GPL v2.

# References

- [ST7703 Datasheet](docs/ST7703_DS_v01_20160128.pdf)
- [GX040HD Panel Specification](docs/GX040HD-30MB-A1.pdf)
- [Initialization Sequence](docs/ST7703_QV040YNQ-N80_IPS_code_2power_4Lane_V1.0_20250611.txt)
- [Raspberry Pi Linux ST7703 Driver Source](https://github.com/raspberrypi/linux/blob/rpi-6.6.y/drivers/gpu/drm/panel/panel-sitronix-st7703.c)
- [Linux DRM Panel Documentation](https://www.kernel.org/doc/html/latest/gpu/drm-kms-helpers.html#panel-helper-reference)