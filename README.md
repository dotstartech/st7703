we are working on the st7703 display support for raspberry pi cm4

here is semi working device tree object

```
/dts-v1/;
/plugin/;

&{/} {
    fragment@0 {
        target-path = "/"; /* or use a valid parent node path in your base DT */
        overlay {
            panel {
                compatible = "sitronix,st7703";

                reset-gpios = <&gpio 23 0>;

                data-lanes = <0 1 2 3>;
                bus-format = "rgb888";
                default-bits-per-pixel = <24>;
                status = "okay";

                display-timings {
                    native-mode = <&mode0>;
                    mode0: mode0 {
                        clock-frequency = <72000000>;
                        hactive = <720>;
                        vactive = <720>;
                        hsync-len = <10>;
                        hfront-porch = <20>;
                        hback-porch = <20>;
                        vsync-len = <2>;
                        vfront-porch = <10>;
                        vback-porch = <10>;
                        refresh = <60>;
                    };
                };
            };
        };
    };
};
```

here is how to compile it
```
sudo dtc -@ -I dts -O dtb -o st7703.dtbo st7703-overlay.dts
```

Here is a command how to copy new overlay file to the boot
```
sudo cp st7703.dtbo /boot/firmware/overlays/
```

Here is the command to mount cm4 emmc
```
 sudo ./rpiboot -d mass-storage-gadget64
```

## Working Solution

### 1. Build and install ST7703 panel driver (out-of-tree)
```bash
sudo bash scripts/build_st7703_module.sh
```

### 2. Create working overlay
The overlay must be structured correctly to avoid device tree conflicts. Use this working version:

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
    
    port {
        endpoint {
            data-lanes = <0 1 2 3>;
        };
    };
    
    panel@0 {
        compatible = "xingbangda,xbd599", "sitronix,st7703";
        reg = <0>;
        reset-gpios = <&gpio 23 1>;
        
        port {
            endpoint {
                data-lanes = <0 1 2 3>;
            };
        };
        
        display-timings {
            timing0 {
                clock-frequency = <41400000>;
                hactive = <720>;
                vactive = <720>;
                hsync-len = <20>;
                hfront-porch = <80>;
                hback-porch = <80>;
                vsync-len = <4>;
                vfront-porch = <30>;
                vback-porch = <12>;
            };
        };
    };
};
```

### 3. Compile and install overlay
```bash
sudo dtc -@ -I dts -O dtb -o st7703.dtbo st7703-overlay.dts
sudo cp st7703.dtbo /boot/firmware/overlays/
```

### 4. Configure system
Ensure `/boot/firmware/config.txt` contains:
```
display_auto_detect=0
dtoverlay=vc4-kms-v3d
dtoverlay=st7703
```

### 5. Apply overlay and test
```bash
sudo dtoverlay st7703
sudo dtoverlay -l
dmesg | grep -i -e dsi -e st7703 -e panel -e vc4
modetest -M vc4
```

## Troubleshooting Notes

- **Driver compatibility**: Use `"xingbangda,xbd599", "sitronix,st7703"` compatible strings
- **GPIO flags**: Try `reset-gpios = <&gpio 23 1>` (active low) or `reset-gpios = <&gpio 23 0>` (active high)
- **Display timings**: Based on panel spec: 720x720, ~41.4MHz, HSA=20, HFP=80, HBP=80, VSA=4, VFP=30, VBP=12
- **Power supply**: Fix undervoltage warnings before testing
- **Cross-compilation**: Use `TARGET_KVER=6.12.25+rpt-rpi-v8` on Pi 5 to build for CM4



# Copy directly Pi5 → CM4
scp -3 admin@10.0.0.50:/tmp/st7703_build/st7703-6.12.25+rpt-rpi-v8.tgz \
    admin@10.0.0.24:/tmp/st7703-6.12.25+rpt-rpi-v8.tgz

# Install on CM4
ssh admin@10.0.0.24 'sudo tar -C / -xzf /tmp/st7703-6.12.25+rpt-rpi-v8.tgz && sudo depmod -a 6.12.25+rpt-rpi-v8'
ssh admin@10.0.0.24 'sudo modprobe panel-sitronix-st7703 || true'

# Verify and reboot if needed
ssh admin@10.0.0.24 'modinfo panel-sitronix-st7703; ls /boot/firmware/overlays/st7703.dtbo'
ssh admin@10.0.0.24 'sudo reboot'