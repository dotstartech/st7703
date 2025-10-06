# Configuration Examples for GX040HD-30MB-A1

## Raspberry Pi Config.txt Setup

Add these lines to `/boot/firmware/config.txt`:

```ini
# Disable auto-detection to use custom panel
display_auto_detect=0

# Enable VC4 graphics driver
dtoverlay=vc4-kms-v3d

# Enable GX040HD panel overlay
dtoverlay=st7703-gx040hd

# Optional: Set GPU memory split
gpu_mem=128

# Optional: Disable overscan
disable_overscan=1
```

## Alternative GPIO Configuration

If you need to use a different GPIO pin for reset, create a custom overlay:

```dts
/dts-v1/;
/plugin/;

/ {
    compatible = "brcm,bcm2711";
};

&dsi0 {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;

    panel@0 {
        compatible = "gx040hd,gx040hd-30mb-a1";
        reg = <0>;
        reset-gpios = <&gpio 25 1>;  /* Use GPIO 25 instead of 23 */
        
        vcc-supply = <&vcc_3v3>;
        iovcc-supply = <&vcc_3v3>;
        
        /* Rest of configuration... */
    };
};
```

## X11 Configuration

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

## Wayland Configuration

For Wayland compositors, the panel should work automatically once the kernel driver is loaded.

## Testing Commands

```bash
# Check if panel is detected
cat /sys/class/drm/card*/status

# Test pattern display
modetest -M vc4 -s <connector_id>:720x720

# Check framebuffer
fbset -fb /dev/fb0

# Display test image
fbi -T 1 -d /dev/fb0 test_image.png
```

## Power Management

The panel supports automatic power management. You can manually control it:

```bash
# Turn display off
echo "off" > /sys/class/drm/card0-DSI-1/status

# Turn display on
echo "on" > /sys/class/drm/card0-DSI-1/status
```

## Brightness Control

If using a backlight controller, add to device tree:

```dts
backlight {
    compatible = "pwm-backlight";
    pwms = <&pwm0 0 50000>;
    brightness-levels = <0 4 8 16 32 64 128 255>;
    default-brightness-level = <6>;
    power-supply = <&vcc_3v3>;
};
```

## Performance Tuning

For optimal performance:

```bash
# Set CPU governor to performance
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

# Increase GPU memory
# Add to config.txt: gpu_mem=256

# Optimize for low latency
echo 1 > /sys/kernel/debug/dri/0/vc4_dsi/dsi0_enable_async
```