/*
 * omap-abe.h
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __OMAP_ABE_PRIV_H__
#define __OMAP_ABE_PRIV_H__

#include <sound/soc.h>
#include <linux/irqreturn.h>

#include "abe/abe.h"
#include "abe/abe_gain.h"
#include "abe/abe_aess.h"

#define OMAP_ABE_FRONTEND_DAI_MEDIA		0
#define OMAP_ABE_FRONTEND_DAI_MEDIA_CAPTURE	1
#define OMAP_ABE_FRONTEND_DAI_VOICE		2
#define OMAP_ABE_FRONTEND_DAI_TONES		3
#define OMAP_ABE_FRONTEND_DAI_VIBRA		4
#define OMAP_ABE_FRONTEND_DAI_MODEM		5
#define OMAP_ABE_FRONTEND_DAI_LP_MEDIA	6
#define OMAP_ABE_FRONTEND_DAI_NUM		7

/* This must currently match the BE order in DSP */
#define OMAP_ABE_DAI_PDM_UL			0
#define OMAP_ABE_DAI_PDM_DL1			1
#define OMAP_ABE_DAI_PDM_DL2			2
#define OMAP_ABE_DAI_PDM_VIB			3
#define OMAP_ABE_DAI_BT_VX			4
#define OMAP_ABE_DAI_MM_FM			5
#define OMAP_ABE_DAI_MODEM			6
#define OMAP_ABE_DAI_DMIC0			7
#define OMAP_ABE_DAI_DMIC1			8
#define OMAP_ABE_DAI_DMIC2			9
#define OMAP_ABE_DAI_VXREC			10
#define OMAP_ABE_DAI_ECHO			11
#define OMAP_ABE_DAI_NUM			12

#define OMAP_ABE_BE_PDM_DL1		"PDM-DL1"
#define OMAP_ABE_BE_PDM_UL1		"PDM-UL1"
#define OMAP_ABE_BE_PDM_DL2		"PDM-DL2"
#define OMAP_ABE_BE_PDM_VIB		"PDM-VIB"
#define OMAP_ABE_BE_BT_VX_UL		"BT-VX-UL"
#define OMAP_ABE_BE_BT_VX_DL		"BT-VX-DL"
#define OMAP_ABE_BE_MM_EXT0		"FM-EXT"
#define OMAP_ABE_BE_MM_EXT0_UL		"FM-EXT-UL"
#define OMAP_ABE_BE_MM_EXT0_DL		"FM-EXT-DL"
#define OMAP_ABE_BE_MM_EXT1		"MODEM-EXT"
#define OMAP_ABE_BE_DMIC0		"DMIC0"
#define OMAP_ABE_BE_DMIC1		"DMIC1"
#define OMAP_ABE_BE_DMIC2		"DMIC2"
#define OMAP_ABE_BE_VXREC		"VXREC"
#define OMAP_ABE_BE_ECHO		"ECHO"

/* Values for virtual mixer */
enum omap_abe_virtual_mix {
	MIX_SWITCH_PDM_DL1 = 0,
	MIX_SWITCH_PDM_DL2,
	MIX_SWITCH_BT_VX_DL,
	MIX_SWITCH_MM_EXT_DL,
	MIX_DL1_MONO,
	MIX_DL2_MONO,
	MIX_AUDUL_MONO,
};

/* Virtual register ids storing enabled mixer inputs */
enum omap_abe_mix {
	OMAP_ABE_VIRTUAL_SWITCH = 0,
	OMAP_ABE_MIX,
	OMAP_ABE_MIX_LAST,
};

/* Virtual register ids storing current mux selection */
enum omap_abe_mux {
	MUX_MM_UL10 = OMAP_ABE_MIX_LAST,
	MUX_MM_UL11,
	MUX_MM_UL12,
	MUX_MM_UL13,
	MUX_MM_UL14,
	MUX_MM_UL15,
	MUX_MM_UL16,
	MUX_MM_UL17,
	MUX_MM_UL20,
	MUX_MM_UL21,
	MUX_VX_UL0,
	MUX_VX_UL1,
	OMAP_ABE_MUX_LAST,
};

/* Virtual register ids used for OPP calculation */
enum omap_abe_widget {
	/* ABE AIF Frontend Widgets */
	OMAP_ABE_AIF_TONES_DL = OMAP_ABE_MUX_LAST,
	OMAP_ABE_AIF_VX_DL,
	OMAP_ABE_AIF_VX_UL,
	OMAP_ABE_AIF_MM_UL1,
	OMAP_ABE_AIF_MM_UL2,
	OMAP_ABE_AIF_MM_DL,
	OMAP_ABE_AIF_MM_DL_LP = OMAP_ABE_AIF_MM_DL,
	OMAP_ABE_AIF_VIB_DL,
	OMAP_ABE_AIF_MODEM_DL,
	OMAP_ABE_AIF_MODEM_UL,

	/* ABE AIF Backend Widgets */
	OMAP_ABE_AIF_PDM_UL1,
	OMAP_ABE_AIF_PDM_DL1,
	OMAP_ABE_AIF_PDM_DL2,
	OMAP_ABE_AIF_PDM_VIB,
	OMAP_ABE_AIF_BT_VX_UL,
	OMAP_ABE_AIF_BT_VX_DL,
	OMAP_ABE_AIF_MM_EXT_UL,
	OMAP_ABE_AIF_MM_EXT_DL,
	OMAP_ABE_AIF_DMIC0,
	OMAP_ABE_AIF_DMIC1,
	OMAP_ABE_AIF_DMIC2,
	OMAP_ABE_AIF_VXREC,
	OMAP_ABE_AIF_ECHO,

	/* ABE ROUTE_UL MUX Widgets */
	OMAP_ABE_MUX_UL00,
	OMAP_ABE_MUX_UL01,
	OMAP_ABE_MUX_UL02,
	OMAP_ABE_MUX_UL03,
	OMAP_ABE_MUX_UL04,
	OMAP_ABE_MUX_UL05,
	OMAP_ABE_MUX_UL10,
	OMAP_ABE_MUX_UL11,
	OMAP_ABE_MUX_VX00,
	OMAP_ABE_MUX_VX01,

	/* ABE Volume and Mixer Widgets */
	OMAP_ABE_MIXER_DL1,
	OMAP_ABE_MIXER_DL2,
	OMAP_ABE_VOLUME_DL1,
	OMAP_ABE_MIXER_AUDIO_UL,
	OMAP_ABE_MIXER_VX_REC,
	OMAP_ABE_MIXER_SDT,
	OMAP_ABE_MIXER_ECHO,
	OMAP_ABE_VSWITCH_DL1_PDM1,
	OMAP_ABE_VSWITCH_DL1_PDM2,
	OMAP_ABE_VSWITCH_DL1_BT_VX,
	OMAP_ABE_VSWITCH_DL1_MM_EXT,
	OMAP_ABE_WIDGET_LAST,
};

#define OMAP_ABE_NUM_MONO_MIXERS	(MIX_AUDUL_MONO - MIX_DL1_MONO + 1)
#define OMAP_ABE_MIXER(x)		(x)
#define OMAP_ABE_MIX_SHIFT(x)		(x - OMAP_AESS_MIXDL1_MM_DL)
#define OMAP_ABE_MUX(x)			(x + OMAP_ABE_MIX_LAST)
#define OMAP_ABE_WIDGET(x)		(x + OMAP_ABE_MUX_LAST)
#define OMAP_ABE_NUM_WIDGETS		(OMAP_ABE_WIDGET_LAST - \
					 OMAP_ABE_MUX_LAST + 1)
#define OMAP_ABE_NUM_DAPM_REG		OMAP_ABE_WIDGET_LAST
#define OMAP_ABE_ROUTES_UL		16

/* Firmware coefficients and equalizers */
#define OMAP_ABE_MAX_FW_SIZE		(1024 * 128)
#define OMAP_ABE_MAX_COEFF_SIZE	(1024 * 4)
#define OMAP_ABE_COEFF_NAME_SIZE	20
#define OMAP_ABE_COEFF_TEXT_SIZE	20
#define OMAP_ABE_COEFF_NUM_TEXTS	10
#define OMAP_ABE_MAX_EQU		10
#define OMAP_ABE_MAX_PROFILES	30

#define OMAP_ABE_OPP_25		0
#define OMAP_ABE_OPP_50		1
#define OMAP_ABE_OPP_100		2
#define OMAP_ABE_OPP_COUNT	3

#define OMAP_ABE_IO_RESOURCES	5

/*
 * ABE loadable coefficients.
 * The coefficient and their mixer configurations are loaded with the firmware
 * blob duing probe().
 */

struct coeff_config {
	char name[OMAP_ABE_COEFF_NAME_SIZE];
	u32 count;
	u32 coeff;
	char texts[OMAP_ABE_COEFF_NUM_TEXTS][OMAP_ABE_COEFF_TEXT_SIZE];
};

/*
 * ABE Firmware Header.
 * The ABE firmware blob has a header that describes each data section. This
 * way we can store coefficients etc in the firmware.
 */
struct fw_header {
	u32 magic;			/* magic number */
	u32 crc;			/* optional crc */
	u32 firmware_size;	/* payload size */
	u32 coeff_size;		/* payload size */
	u32 coeff_version;	/* coefficent version */
	u32 firmware_version;	/* min version of ABE firmware required */
	u32 num_equ;		/* number of equalizers */
};

struct abe_opp_req {
	struct device *dev;
	struct list_head node;
	int opp;
};

struct omap_abe_debugfs {
	/* its intended we can switch on/off individual debug items */
	u32 format1; /* TODO: match flag names here to debug format flags */
	u32 format2;
	u32 format3;

	/* ABE DMA buffer */
	u32 buffer_bytes;
	u32 circular;
	u32 buffer_msecs;  /* size of buffer in secs */
	u32 elem_bytes;
	dma_addr_t buffer_addr;
	wait_queue_head_t wait;
	size_t reader_offset;
	size_t dma_offset;
	int complete;
	char *buffer;
	struct omap_pcm_dma_data *dma_data;
	int dma_ch;
	int dma_req;

	/* debugfs */
	struct dentry *d_root;
	struct dentry *d_fmt1;
	struct dentry *d_fmt2;
	struct dentry *d_fmt3;
	struct dentry *d_size;
	struct dentry *d_data;
	struct dentry *d_circ;
	struct dentry *d_elem_bytes;
	struct dentry *d_opp;
};

struct omap_abe_dc_offset {
	/* DC offset cancellation */
	int power_mode;
	u32 hsl;
	u32 hsr;
	u32 hfl;
	u32 hfr;
};

struct omap_abe_opp {
	struct mutex mutex;
	struct mutex req_mutex;
	int level;
	unsigned long freqs[OMAP_ABE_OPP_COUNT];
	u32 widget[OMAP_ABE_NUM_DAPM_REG + 1];
	struct list_head req;
	int req_count;
};

struct omap_abe_modem {
	struct snd_pcm_substream *substream[2];
	struct snd_soc_dai *dai;
};

struct omap_abe_mmap {
	int first_irq;
};

struct omap_abe_equ {
	s32 *equ[OMAP_ABE_MAX_EQU];
	int profile[OMAP_ABE_MAX_EQU];
	struct soc_enum senum[OMAP_ABE_MAX_EQU];
	struct snd_kcontrol_new kcontrol[OMAP_ABE_MAX_EQU];
	struct coeff_config *texts;
};

struct omap_abe_dai {
	struct omap_abe_port *port[OMAP_ABE_MAX_PORT_ID + 1];
	int num_active;
	int num_suspended;
};

struct omap_abe_mixer {
	int mono[OMAP_ABE_NUM_MONO_MIXERS];
	u16 route_ul[16];
};

/*
 * ABE private data.
 */
struct omap_abe {
	struct device *dev;
	struct omap_aess *aess;

	struct clk *clk;
	void __iomem *io_base[OMAP_ABE_IO_RESOURCES];
	int irq;
	int active;
	struct mutex mutex;

	int (*get_context_loss_count)(struct device *dev);
	int (*device_scale)(struct device *target_dev,
			    unsigned long rate);
	int context_loss;

	struct omap_abe_opp opp;
	struct omap_abe_dc_offset dc_offset;
	struct omap_abe_modem modem;
	struct omap_abe_mmap mmap;
	struct omap_abe_equ equ;
	struct omap_abe_dai dai;
	struct omap_abe_mixer mixer;

	/* coefficients */
	struct fw_header hdr;
	u32 *firmware;

#ifdef CONFIG_DEBUG_FS
	struct omap_abe_debugfs debugfs;
#endif
};

/* External API for client component drivers */

/* Power Management */
void omap_abe_pm_shutdown(struct snd_soc_platform *platform);
void omap_abe_pm_get(struct snd_soc_platform *platform);
void omap_abe_pm_put(struct snd_soc_platform *platform);
void omap_abe_pm_set_mode(struct snd_soc_platform *platform, int mode);

/* Operating Point */
int omap_abe_opp_new_request(struct snd_soc_platform *platform,
		struct device *dev, int opp);
int omap_abe_opp_free_request(struct snd_soc_platform *platform,
		struct device *dev);

/* DC Offset */
void omap_abe_dc_set_hs_offset(struct snd_soc_platform *platform,
	int left, int right, int step_mV);
void omap_abe_dc_set_hf_offset(struct snd_soc_platform *platform,
	int left, int right);

/* Internal OMAP SoC API declarations */

/* forward declarations */
struct omap_abe;
struct snd_ctl_elem_info;
struct snd_kcontrol;
struct snd_soc_dai;
struct snd_soc_dapm_context;
struct snd_soc_platform;

/* omap-abe-mixer.c */
unsigned int abe_mixer_read(struct snd_soc_platform *platform,
		unsigned int reg);
int abe_mixer_write(struct snd_soc_platform *platform, unsigned int reg,
		unsigned int val);
int abe_mixer_enable_mono(struct omap_abe *abe, int id, int enable);
int abe_mixer_set_equ_profile(struct omap_abe *abe,
		unsigned int id, unsigned int profile);
int abe_mixer_add_widgets(struct snd_soc_platform *platform);

/* omap-abe-pcm.c */
extern struct snd_soc_dai_driver omap_abe_dai[7];

/* omap-abe-pm.c */
int abe_pm_save_context(struct omap_abe *abe);
int abe_pm_restore_context(struct omap_abe *abe);
#ifdef CONFIG_PM
int abe_pm_suspend(struct snd_soc_dai *dai);
int abe_pm_resume(struct snd_soc_dai *dai);
#endif
/* use everywhere except probe(): */
int omap_abe_pm_runtime_get_sync(struct omap_abe *abe);
int omap_abe_pm_runtime_put_sync(struct omap_abe *abe);
int omap_abe_pm_runtime_put_sync_suspend(struct omap_abe *abe);

/* omap-abe-mmap.c */
irqreturn_t abe_irq_handler(int irq, void *dev_id);
extern struct snd_pcm_ops omap_aess_pcm_ops;
int abe_opp_recalc_level(struct omap_abe *abe);

/* omap-abe-opp.c */
int abe_opp_init_initial_opp(struct omap_abe *abe);
int abe_opp_set_level(struct omap_abe *abe, int opp);
int abe_opp_stream_event(struct snd_soc_dapm_context *dapm, int event);

/* omap-abe-dbg.c */
void abe_init_debugfs(struct omap_abe *abe);
void abe_cleanup_debugfs(struct omap_abe *abe);

#endif	/* End of __OMAP_MCPDM_H__ */
