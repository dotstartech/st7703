// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for panels based on Sitronix ST7703 controller, such as:
 *
 * - Rocktech jh057n00900 5.5" MIPI-DSI panel
 * - Xingbangda XBD599 5.99" MIPI-DSI panel
 * - GX040HD-30MB-A1 4.0" MIPI-DSI panel
 *
 * Copyright (C) Purism SPC 2019
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/media-bus-format.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/display_timing.h>
#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#define DRV_NAME "panel-sitronix-st7703"

/* Manufacturer specific Commands send via DSI */
#define ST7703_CMD_ALL_PIXEL_OFF 0x22
#define ST7703_CMD_ALL_PIXEL_ON	 0x23
#define ST7703_CMD_SETAPID	 0xB1
#define ST7703_CMD_SETDISP	 0xB2
#define ST7703_CMD_SETRGBIF	 0xB3
#define ST7703_CMD_SETCYC	 0xB4
#define ST7703_CMD_SETBGP	 0xB5
#define ST7703_CMD_SETVCOM	 0xB6
#define ST7703_CMD_SETOTP	 0xB7
#define ST7703_CMD_SETPOWER_EXT	 0xB8
#define ST7703_CMD_SETEXTC	 0xB9
#define ST7703_CMD_SETMIPI	 0xBA
#define ST7703_CMD_SETVDC	 0xBC
#define ST7703_CMD_UNKNOWN_BF	 0xBF
#define ST7703_CMD_SETSCR	 0xC0
#define ST7703_CMD_SETPOWER	 0xC1
#define ST7703_CMD_SETECO	 0xC6
#define ST7703_CMD_SETIO	 0xC7
#define ST7703_CMD_SETCABC	 0xC8
#define ST7703_CMD_SETPANEL	 0xCC
#define ST7703_CMD_SETGAMMA	 0xE0
#define ST7703_CMD_SETEQ	 0xE3
#define ST7703_CMD_SETGIP1	 0xE9
#define ST7703_CMD_SETGIP2	 0xEA
#define ST7703_CMD_UNKNOWN_EF	 0xEF

struct st7703 {
	struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
	struct regulator *vcc;
	struct regulator *iovcc;

	struct dentry *debugfs;
	const struct st7703_panel_desc *desc;
	enum drm_panel_orientation orientation;
};

struct st7703_panel_desc {
	const struct drm_display_mode *mode;
	unsigned int lanes;
	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	void (*init_sequence)(struct mipi_dsi_multi_context *dsi_ctx);
};

static inline struct st7703 *panel_to_st7703(struct drm_panel *panel)
{
	return container_of(panel, struct st7703, panel);
}

static void jh057n_init_sequence(struct mipi_dsi_multi_context *dsi_ctx)
{
	/*
	 * Init sequence was supplied by the panel vendor. Most of the commands
	 * resemble the ST7703 but the number of parameters often don't match
	 * so it's likely a clone.
	 */
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETEXTC,
					 0xF1, 0x12, 0x83);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETRGBIF,
					 0x10, 0x10, 0x05, 0x05, 0x03, 0xFF, 0x00, 0x00,
					 0x00, 0x00);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETSCR,
					 0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x08, 0x70,
					 0x00);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETVDC, 0x4E);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETPANEL, 0x0B);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETCYC, 0x80);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETDISP, 0xF0, 0x12,
					 0x30);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETEQ,
					 0x07, 0x07, 0x0B, 0x0B, 0x03, 0x0B, 0x00, 0x00,
					 0x00, 0x00, 0xFF, 0x00, 0xC0, 0x10);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETBGP, 0x08, 0x08);
	mipi_dsi_msleep(dsi_ctx, 20);

	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETVCOM, 0x3F, 0x3F);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_UNKNOWN_BF, 0x02, 0x11,
					 0x00);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETGIP1,
					 0x82, 0x10, 0x06, 0x05, 0x9E, 0x0A, 0xA5, 0x12,
					 0x31, 0x23, 0x37, 0x83, 0x04, 0xBC, 0x27, 0x38,
					 0x0C, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0C, 0x00,
					 0x03, 0x00, 0x00, 0x00, 0x75, 0x75, 0x31, 0x88,
					 0x88, 0x88, 0x88, 0x88, 0x88, 0x13, 0x88, 0x64,
					 0x64, 0x20, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
					 0x02, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	mipi_dsi_generic_write_seq_multi(dsi_ctx, ST7703_CMD_SETGIP2,
					 0x02, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x02, 0x46, 0x02, 0x88,
					 0x88, 0x88, 0x88, 0x88, 0x88, 0x64, 0x88, 0x13,
					 0x57, 0x13, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
					 0x75, 0x88, 0x23, 0x14, 0x00, 0x00, 0x02, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x0A,
					 0xA5, 0x00, 0x00, 0x00, 0x00);

	/* Adjust the gamma characteristics of the panel. */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETGAMMA,
				     0x00, 0x09, 0x0D, 0x23, 0x27, 0x3C, 0x41, 0x35,
				     0x07, 0x0D, 0x0E, 0x12, 0x13, 0x10, 0x12, 0x12,
				     0x18, 0x00, 0x09, 0x0D, 0x23, 0x27, 0x3C, 0x41,
				     0x35, 0x07, 0x0D, 0x0E, 0x12, 0x13, 0x10, 0x12,
				     0x12, 0x18);
}

static const struct drm_display_mode jh057n00900_mode = {
	.hdisplay    = 720,
	.hsync_start = 720 + 90,
	.hsync_end   = 720 + 90 + 20,
	.htotal	     = 720 + 90 + 20 + 20,
	.vdisplay    = 1440,
	.vsync_start = 1440 + 20,
	.vsync_end   = 1440 + 20 + 4,
	.vtotal	     = 1440 + 20 + 4 + 12,
	.clock	     = 75276,
	.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
	.width_mm    = 65,
	.height_mm   = 130,
};

static const struct st7703_panel_desc jh057n00900_panel_desc = {
	.mode = &jh057n00900_mode,
	.lanes = 4,
	.mode_flags = MIPI_DSI_MODE_VIDEO |
		MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_VIDEO_SYNC_PULSE,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = jh057n_init_sequence,
};

static void xbd599_init_sequence(struct mipi_dsi_multi_context *dsi_ctx)
{
	/*
	 * Init sequence was supplied by the panel vendor.
	 */

	/* Magic sequence to unlock user commands below. */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETEXTC, 0xF1, 0x12, 0x83);

	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETMIPI,
				     0x33, /* VC_main = 0, Lane_Number = 3 (4 lanes) */
				     0x81, /* DSI_LDO_SEL = 1.7V, RTERM = 90 Ohm */
				     0x05, /* IHSRX = x6 (Low High Speed driving ability) */
				     0xF9, /* TX_CLK_SEL = fDSICLK/16 */
				     0x0E, /* HFP_OSC (min. HFP number in DSI mode) */
				     0x0E, /* HBP_OSC (min. HBP number in DSI mode) */
				     /* The rest is undocumented in ST7703 datasheet */
				     0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x44, 0x25, 0x00, 0x90, 0x0a, 0x00, 0x00, 0x01,
				     0x4f, 0x01, 0x00, 0x00, 0x37);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETPOWER_EXT, 0x25, 0x22,
				     0xf0, 0x63);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_UNKNOWN_BF, 0x02, 0x11, 0x00);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETRGBIF, 0x10, 0x10, 0x28,
				     0x28, 0x03, 0xff, 0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETSCR, 0x73, 0x73, 0x50, 0x50,
				     0x00, 0x00, 0x12, 0x70, 0x00);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETVDC, 0x46);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETPANEL, 0x0b);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETCYC, 0x80);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETDISP, 0x3c, 0x12, 0x30);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETEQ, 0x07, 0x07, 0x0b, 0x0b,
				     0x03, 0x0b, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
				     0xc0, 0x10);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETPOWER, 0x36, 0x00, 0x32,
				     0x32, 0x77, 0xf1, 0xcc, 0xcc, 0x77, 0x77, 0x33,
				     0x33);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETBGP, 0x0a, 0x0a);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETVCOM, 0xb2, 0xb2);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETGIP1,
				     0xc8, 0x10, 0x0a, 0x10, 0x0f, 0xa1, 0x80, 0x12,
				     0x31, 0x23, 0x47, 0x86, 0xa1, 0x80, 0x47, 0x08,
				     0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x0d, 0x00, 0x00, 0x00, 0x48, 0x02, 0x8b, 0xaf,
				     0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x48,
				     0x13, 0x8b, 0xaf, 0x57, 0x13, 0x88, 0x88, 0x88,
				     0x88, 0x88, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETGIP2,
				     0x96, 0x12, 0x01, 0x01, 0x01, 0x78, 0x02, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x4f, 0x31, 0x8b, 0xa8,
				     0x31, 0x75, 0x88, 0x88, 0x88, 0x88, 0x88, 0x4f,
				     0x20, 0x8b, 0xa8, 0x20, 0x64, 0x88, 0x88, 0x88,
				     0x88, 0x88, 0x23, 0x00, 0x00, 0x01, 0x02, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xa1,
				     0x80, 0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETGAMMA, 0x00, 0x0a, 0x0f,
				     0x29, 0x3b, 0x3f, 0x42, 0x39, 0x06, 0x0d, 0x10,
				     0x13, 0x15, 0x14, 0x15, 0x10, 0x17, 0x00, 0x0a,
				     0x0f, 0x29, 0x3b, 0x3f, 0x42, 0x39, 0x06, 0x0d,
				     0x10, 0x13, 0x15, 0x14, 0x15, 0x10, 0x17);
}

static const struct drm_display_mode xbd599_mode = {
	.hdisplay    = 720,
	.hsync_start = 720 + 40,
	.hsync_end   = 720 + 40 + 40,
	.htotal	     = 720 + 40 + 40 + 40,
	.vdisplay    = 1440,
	.vsync_start = 1440 + 18,
	.vsync_end   = 1440 + 18 + 10,
	.vtotal	     = 1440 + 18 + 10 + 17,
	.clock	     = 69000,
	.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
	.width_mm    = 68,
	.height_mm   = 136,
};

static const struct st7703_panel_desc xbd599_desc = {
	.mode = &xbd599_mode,
	.lanes = 4,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = xbd599_init_sequence,
};

static void gx040hd_init_sequence(struct mipi_dsi_multi_context *dsi_ctx)
{
	/*
	 * Init sequence for GX040HD-30MB-A1 4.0" 720x720 IPS LCD panel
	 * based on ST7703 controller. Init sequence extracted from vendor BSP.
	 */

	/* Magic sequence to unlock user commands */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETEXTC, 0xF1, 0x12, 0x83);

	/* Set MIPI DSI configuration */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETMIPI,
				     0x33, /* 4Lane */
				     0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00, 
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00, 
				     0x90, 0x0A, 0x00, 0x00, 0x01, 0x4F, 0x01, 0x00, 
				     0x00, 0x37);

	/* Set Power Control extension */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETPOWER_EXT, 0x25, 0x22,
				     0xF0, 0x63);

	/* Set unknown BF register */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_UNKNOWN_BF, 0x02, 0x11, 0x00);

	/* Set RGB interface */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETRGBIF, 0x10, 0x10, 0x28,
				     0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00);

	/* Set source control register */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETSCR, 0x73, 0x73, 0x50, 0x50,
				     0x00, 0x00, 0x12, 0x70, 0x00);

	/* Set VDC voltage */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETVDC, 0x46);

	/* Set panel control */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETPANEL, 0x0B);

	/* Set panel inversion */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETCYC, 0x80);

	/* Set display resolution and timing */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETDISP, 0x3C, 0x12, 0x30);

	/* Set EQ timing control */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETEQ, 0x07, 0x07, 0x0B, 0x0B,
				     0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00,
				     0xC0, 0x10);

	/* Set power control */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETPOWER, 0x36, 0x00, 0x32,
				     0x32, 0x77, 0xF1, 0xCC, 0xCC, 0x77, 0x77, 0x33,
				     0x33);

	/* Set BGP voltage */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETBGP, 0x0A, 0x0A);

	/* Set VCOM voltage */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETVCOM, 0xB2, 0xB2);

	/* Set GIP1 timing control */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETGIP1,
				     0xC8, 0x10, 0x0A, 0x10, 0x0F, 0xA1, 0x80, 0x12,
				     0x31, 0x23, 0x47, 0x86, 0xA1, 0x80, 0x47, 0x08,
				     0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x0D, 0x00, 0x00, 0x00, 0x48, 0x02, 0x8B, 0xAF,
				     0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x48,
				     0x13, 0x8B, 0xAF, 0x57, 0x13, 0x88, 0x88, 0x88,
				     0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

	/* Set GIP2 timing control */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETGIP2,
				     0x96, 0x12, 0x01, 0x01, 0x01, 0x78, 0x02, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x4F, 0x31, 0x8B, 0xA8,
				     0x31, 0x75, 0x88, 0x88, 0x88, 0x88, 0x88, 0x4F,
				     0x20, 0x8B, 0xA8, 0x20, 0x64, 0x88, 0x88, 0x88,
				     0x88, 0x88, 0x23, 0x00, 0x00, 0x01, 0x02, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xA1,
				     0x80, 0x00, 0x00, 0x00, 0x00);

	/* Set gamma correction */
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, ST7703_CMD_SETGAMMA,
				     0x00, 0x0A, 0x0F, 0x29, 0x3B, 0x3F, 0x42, 0x39,
				     0x06, 0x0D, 0x10, 0x13, 0x15, 0x14, 0x15, 0x10,
				     0x17, 0x00, 0x0A, 0x0F, 0x29, 0x3B, 0x3F, 0x42,
				     0x39, 0x06, 0x0D, 0x10, 0x13, 0x15, 0x14, 0x15,
				     0x10, 0x17);
}

static const struct drm_display_mode gx040hd_mode = {
	.hdisplay    = 720,
	.hsync_start = 720 + 80,  /* HFP */
	.hsync_end   = 720 + 80 + 20,  /* HSA */
	.htotal	     = 720 + 80 + 20 + 80,  /* HBP */
	.vdisplay    = 720,
	.vsync_start = 720 + 30,  /* VFP */
	.vsync_end   = 720 + 30 + 4,   /* VSA */
	.vtotal	     = 720 + 30 + 4 + 12,   /* VBP */
	.clock	     = 41400,  /* 41.4MHz */
	.flags	     = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
	.width_mm    = 89,  /* 89.6mm according to datasheet */
	.height_mm   = 89,  /* 89.6mm according to datasheet */
};

static const struct st7703_panel_desc gx040hd_desc = {
	.mode = &gx040hd_mode,
	.lanes = 4,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
		      MIPI_DSI_MODE_NO_EOT_PACKET | MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = gx040hd_init_sequence,
};

static int st7703_enable(struct drm_panel *panel)
{
	struct st7703 *ctx = panel_to_st7703(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = dsi };

	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 250);

	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 50);

	return dsi_ctx.accum_err;
}

static int st7703_disable(struct drm_panel *panel)
{
	struct st7703 *ctx = panel_to_st7703(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = dsi };

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int st7703_unprepare(struct drm_panel *panel)
{
	struct st7703 *ctx = panel_to_st7703(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = dsi };

	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_disable(ctx->iovcc);
	regulator_disable(ctx->vcc);

	return dsi_ctx.accum_err;
}

static int st7703_prepare(struct drm_panel *panel)
{
	struct st7703 *ctx = panel_to_st7703(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = dsi };
	int ret;

	dev_dbg(ctx->dev, "Resetting the panel\n");
	ret = regulator_enable(ctx->vcc);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to enable vcc supply: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(ctx->iovcc);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to enable iovcc supply: %d\n", ret);
		goto disable_vcc;
	}

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(20, 40);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);

	ctx->desc->init_sequence(&dsi_ctx);
	if (dsi_ctx.accum_err)
		goto disable_iovcc;

	dev_dbg(ctx->dev, "Panel init sequence done\n");
	return 0;

disable_iovcc:
	regulator_disable(ctx->iovcc);
disable_vcc:
	regulator_disable(ctx->vcc);
	return ret;
}

static const u32 mantix_bus_formats[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
};

static int st7703_get_modes(struct drm_panel *panel,
			    struct drm_connector *connector)
{
	struct st7703 *ctx = panel_to_st7703(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, ctx->desc->mode);
	if (!mode) {
		dev_err(ctx->dev, "Failed to add mode %ux%u@%u\n",
			ctx->desc->mode->hdisplay, ctx->desc->mode->vdisplay,
			drm_mode_vrefresh(ctx->desc->mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	drm_display_info_set_bus_formats(&connector->display_info,
					 mantix_bus_formats,
					 ARRAY_SIZE(mantix_bus_formats));

	return 1;
}

static enum drm_panel_orientation st7703_get_orientation(struct drm_panel *panel)
{
	struct st7703 *st7703 = panel_to_st7703(panel);

	return st7703->orientation;
}

static const struct drm_panel_funcs st7703_drm_funcs = {
	.disable   = st7703_disable,
	.unprepare = st7703_unprepare,
	.prepare   = st7703_prepare,
	.enable	   = st7703_enable,
	.get_modes = st7703_get_modes,
	.get_orientation = st7703_get_orientation,
};

static int allpixelson_set(void *data, u64 val)
{
	struct st7703 *ctx = data;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	struct mipi_dsi_multi_context dsi_ctx = {.dsi = dsi};

	dev_dbg(ctx->dev, "Setting all pixels on\n");
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, ST7703_CMD_ALL_PIXEL_ON);
	mipi_dsi_msleep(&dsi_ctx, 20);

	return dsi_ctx.accum_err;
}

DEFINE_SIMPLE_ATTRIBUTE(allpixelson_fops, NULL,
			allpixelson_set, "%llu\n");

static void st7703_debugfs_init(struct st7703 *ctx)
{
	ctx->debugfs = debugfs_create_dir(DRV_NAME, NULL);

	debugfs_create_file("allpixelson", 0600, ctx->debugfs, ctx,
			    &allpixelson_fops);
}

static void st7703_debugfs_remove(struct st7703 *ctx)
{
	debugfs_remove_recursive(ctx->debugfs);
	ctx->debugfs = NULL;
}

static int st7703_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct st7703 *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct st7703, panel,
				   &st7703_drm_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio), "Failed to get reset gpio\n");

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	ctx->desc = of_device_get_match_data(dev);

	dsi->mode_flags = ctx->desc->mode_flags;
	dsi->format = ctx->desc->format;
	dsi->lanes = ctx->desc->lanes;

	ctx->vcc = devm_regulator_get(dev, "vcc");
	if (IS_ERR(ctx->vcc))
		return dev_err_probe(dev, PTR_ERR(ctx->vcc), "Failed to request vcc regulator\n");

	ctx->iovcc = devm_regulator_get(dev, "iovcc");
	if (IS_ERR(ctx->iovcc))
		return dev_err_probe(dev, PTR_ERR(ctx->iovcc),
				     "Failed to request iovcc regulator\n");

	ret = of_drm_get_panel_orientation(dsi->dev.of_node, &ctx->orientation);
	if (ret < 0)
		return dev_err_probe(&dsi->dev, ret, "Failed to get orientation\n");

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return ret;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "mipi_dsi_attach failed (%d). Is host ready?\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	dev_info(dev, "%ux%u@%u %ubpp dsi %udl - ready\n",
		 ctx->desc->mode->hdisplay, ctx->desc->mode->vdisplay,
		 drm_mode_vrefresh(ctx->desc->mode),
		 mipi_dsi_pixel_format_to_bpp(dsi->format), dsi->lanes);

	st7703_debugfs_init(ctx);
	return 0;
}

static void st7703_remove(struct mipi_dsi_device *dsi)
{
	struct st7703 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	st7703_debugfs_remove(ctx);
}

static const struct of_device_id st7703_of_match[] = {
	{ .compatible = "gx040hd,gx040hd-30mb-a1", .data = &gx040hd_desc },
	{ .compatible = "rocktech,jh057n00900", .data = &jh057n00900_panel_desc },
	{ .compatible = "xingbangda,xbd599", .data = &xbd599_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, st7703_of_match);

static struct mipi_dsi_driver st7703_driver = {
	.probe	= st7703_probe,
	.remove = st7703_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = st7703_of_match,
	},
};
module_mipi_dsi_driver(st7703_driver);

MODULE_AUTHOR("Guido Günther <agx@sigxcpu.org>");
MODULE_DESCRIPTION("DRM driver for Sitronix ST7703 based MIPI DSI panels");
MODULE_LICENSE("GPL v2");