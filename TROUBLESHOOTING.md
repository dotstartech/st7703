# ST7703 Display Troubleshooting Guide

## Current Issues Identified

Based on your dmesg log analysis:

1. **No ST7703 driver activity** - The ST7703 driver is not being loaded
2. **VC4 DRM reports "Cannot find any crtc or sizes"** - No display detected
3. **Missing MIPI DSI configuration** - Panel not properly connected to DSI interface

## Updated Device Tree Overlay

The new `st7703-overlay.dts` includes:
- Proper MIPI DSI host configuration (`dsi1`)
- Correct panel binding with endpoint connections
- GPIO backlight control (GPIO 18)
- Reset GPIO (GPIO 23)
- Display timing configuration

## Testing Steps

### 1. Compile the Updated Overlay
```bash
sudo dtc -@ -I dts -O dtb -o st7703.dtbo st7703-overlay.dts
```

### 2. Install the Overlay
```bash
sudo cp st7703.dtbo /boot/firmware/overlays/
```

### 3. Enable the Overlay
Add to `/boot/firmware/config.txt`:
```
dtoverlay=st7703
```

### 4. Enable VC4 KMS (if not already enabled)
Add to `/boot/firmware/config.txt`:
```
dtoverlay=vc4-kms-v3d
```

### 5. Reboot and Check
```bash
sudo reboot
dmesg | grep -i st7703
dmesg | grep -i dsi
dmesg | grep -i panel
```

## Expected dmesg Output

After successful setup, you should see:
```
vc4-drm gpu: bound fe204000.dsi (ops vc4_dsi_ops [vc4])
vc4-drm gpu: bound fe700000.dsi (ops vc4_dsi_ops [vc4])
sitronix,st7703: panel found
vc4-drm gpu: [drm] Initialized vc4 0.0.0 for gpu on minor 1
```

## Hardware Connections

Ensure your ST7703 display is connected to:
- **MIPI DSI Data Lines**: D0+, D0-, D1+, D1-, D2+, D2-, D3+, D3-
- **Clock Lines**: CLK+, CLK-
- **Reset GPIO**: GPIO 23
- **Backlight GPIO**: GPIO 18 (optional, can use PWM)
- **Power**: 3.3V and GND

## Common Issues

1. **"Cannot find any crtc or sizes"** - Usually means DSI interface not enabled or panel not detected
2. **No ST7703 messages** - Driver not loaded, check if overlay is enabled
3. **DSI errors** - Check physical connections and power supply

## Debug Commands

```bash
# Check loaded overlays
lsmod | grep st7703

# Check DSI interface
ls /dev/dri/

# Check framebuffer
ls /dev/fb*

# Monitor kernel messages
dmesg -w | grep -E "(st7703|dsi|panel|vc4)"
```

## Next Steps

1. Test the updated overlay
2. Check hardware connections
3. Verify power supply to display
4. Monitor dmesg for any new error messages
