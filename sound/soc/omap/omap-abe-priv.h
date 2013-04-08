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

#ifdef __KERNEL__

#include <sound/soc-fw.h>

#include "aess/abe.h"
#include "aess/abe_gain.h"
#include "aess/abe_aess.h"
#include "aess/abe_mem.h"

#endif

#define OMAP_ABE_FRONTEND_DAI_MEDIA		0
#define OMAP_ABE_FRONTEND_DAI_MEDIA_CAPTURE	1
#define OMAP_ABE_FRONTEND_DAI_VOICE		2
#define OMAP_ABE_FRONTEND_DAI_TONES		3
#define OMAP_ABE_FRONTEND_DAI_MODEM		4
#define OMAP_ABE_FRONTEND_DAI_LP_MEDIA		5
#define OMAP_ABE_FRONTEND_DAI_NUM		6

/* This must currently match the BE order in DSP */
#define OMAP_ABE_DAI_PDM_UL			0
#define OMAP_ABE_DAI_PDM_DL1			1
#define OMAP_ABE_DAI_PDM_DL2			2
#define OMAP_ABE_DAI_BT_VX			3
#define OMAP_ABE_DAI_MM_FM			4
#define OMAP_ABE_DAI_MODEM			5
#define OMAP_ABE_DAI_DMIC0			6
#define OMAP_ABE_DAI_DMIC1			7
#define OMAP_ABE_DAI_DMIC2			8
#define OMAP_ABE_DAI_VXREC			9
#define OMAP_ABE_DAI_NUM			10

#define OMAP_ABE_MIXER(x)		(x)


#define MIX_SWITCH_PDM_DL		OMAP_ABE_MIXER(1)
#define MIX_SWITCH_BT_VX_DL		OMAP_ABE_MIXER(2)
#define MIX_SWITCH_MM_EXT_DL		OMAP_ABE_MIXER(3)
#define MIX_DL1_MONO		OMAP_ABE_MIXER(4)
#define MIX_DL2_MONO		OMAP_ABE_MIXER(5)
#define MIX_AUDUL_MONO		OMAP_ABE_MIXER(6)

#define OMAP_ABE_VIRTUAL_SWITCH	36

#define OMAP_ABE_NUM_MONO_MIXERS	(MIX_AUDUL_MONO - MIX_DL1_MONO + 1)
#define OMAP_ABE_NUM_MIXERS		(MIX_AUDUL_MONO + 1)

#define OMAP_ABE_MUX(x)		(x + 37)

#define MUX_MM_UL10		OMAP_ABE_MUX(0)
#define MUX_MM_UL11		OMAP_ABE_MUX(1)
#define MUX_MM_UL12		OMAP_ABE_MUX(2)
#define MUX_MM_UL13		OMAP_ABE_MUX(3)
#define MUX_MM_UL14		OMAP_ABE_MUX(4)
#define MUX_MM_UL15		OMAP_ABE_MUX(5)
#define MUX_MM_UL20		OMAP_ABE_MUX(6)
#define MUX_MM_UL21		OMAP_ABE_MUX(7)
#define MUX_VX_UL0		OMAP_ABE_MUX(8)
#define MUX_VX_UL1		OMAP_ABE_MUX(9)

#define OMAP_ABE_NUM_MUXES		(MUX_VX_UL1 - MUX_MM_UL10)

#define OMAP_ABE_WIDGET(x)		(x + OMAP_ABE_NUM_MIXERS + OMAP_ABE_NUM_MUXES)

/* ABE AIF Frontend Widgets */
#define OMAP_ABE_AIF_TONES_DL		OMAP_ABE_WIDGET(0)
#define OMAP_ABE_AIF_VX_DL		OMAP_ABE_WIDGET(1)
#define OMAP_ABE_AIF_VX_UL		OMAP_ABE_WIDGET(2)
#define OMAP_ABE_AIF_MM_UL1		OMAP_ABE_WIDGET(3)
#define OMAP_ABE_AIF_MM_UL2		OMAP_ABE_WIDGET(4)
#define OMAP_ABE_AIF_MM_DL		OMAP_ABE_WIDGET(5)
#define OMAP_ABE_AIF_MM_DL_LP		OMAP_ABE_AIF_MM_DL
#define OMAP_ABE_AIF_MODEM_DL		OMAP_ABE_WIDGET(6)
#define OMAP_ABE_AIF_MODEM_UL		OMAP_ABE_WIDGET(7)

/* ABE AIF Backend Widgets */
#define OMAP_ABE_AIF_PDM_UL1		OMAP_ABE_WIDGET(8)
#define OMAP_ABE_AIF_PDM_DL1		OMAP_ABE_WIDGET(9)
#define OMAP_ABE_AIF_PDM_DL2		OMAP_ABE_WIDGET(10)
#define OMAP_ABE_AIF_BT_VX_UL		OMAP_ABE_WIDGET(11)
#define OMAP_ABE_AIF_BT_VX_DL		OMAP_ABE_WIDGET(12)
#define OMAP_ABE_AIF_MM_EXT_UL	OMAP_ABE_WIDGET(13)
#define OMAP_ABE_AIF_MM_EXT_DL	OMAP_ABE_WIDGET(14)
#define OMAP_ABE_AIF_DMIC0		OMAP_ABE_WIDGET(15)
#define OMAP_ABE_AIF_DMIC1		OMAP_ABE_WIDGET(16)
#define OMAP_ABE_AIF_DMIC2		OMAP_ABE_WIDGET(17)

/* ABE ROUTE_UL MUX Widgets */
#define OMAP_ABE_MUX_UL00		OMAP_ABE_WIDGET(18)
#define OMAP_ABE_MUX_UL01		OMAP_ABE_WIDGET(19)
#define OMAP_ABE_MUX_UL02		OMAP_ABE_WIDGET(20)
#define OMAP_ABE_MUX_UL03		OMAP_ABE_WIDGET(21)
#define OMAP_ABE_MUX_UL04		OMAP_ABE_WIDGET(22)
#define OMAP_ABE_MUX_UL05		OMAP_ABE_WIDGET(23)
#define OMAP_ABE_MUX_UL10		OMAP_ABE_WIDGET(24)
#define OMAP_ABE_MUX_UL11		OMAP_ABE_WIDGET(25)
#define OMAP_ABE_MUX_VX00		OMAP_ABE_WIDGET(26)
#define OMAP_ABE_MUX_VX01		OMAP_ABE_WIDGET(27)

/* ABE Volume and Mixer Widgets */
#define OMAP_ABE_MIXER_DL1		OMAP_ABE_WIDGET(28)
#define OMAP_ABE_MIXER_DL2		OMAP_ABE_WIDGET(29)
#define OMAP_ABE_VOLUME_DL1		OMAP_ABE_WIDGET(30)
#define OMAP_ABE_MIXER_AUDIO_UL	OMAP_ABE_WIDGET(31)
#define OMAP_ABE_MIXER_VX_REC		OMAP_ABE_WIDGET(32)
#define OMAP_ABE_MIXER_SDT		OMAP_ABE_WIDGET(33)
#define OMAP_ABE_VSWITCH_DL1_PDM	OMAP_ABE_WIDGET(34)
#define OMAP_ABE_VSWITCH_DL1_BT_VX	OMAP_ABE_WIDGET(35)
#define OMAP_ABE_VSWITCH_DL1_MM_EXT	OMAP_ABE_WIDGET(36)
#define OMAP_ABE_AIF_VXREC		OMAP_ABE_WIDGET(37)

#define OMAP_ABE_NUM_WIDGETS		(OMAP_ABE_AIF_VXREC - OMAP_ABE_AIF_TONES_DL)
#define OMAP_ABE_WIDGET_LAST		OMAP_ABE_AIF_VXREC

#define OMAP_ABE_NUM_DAPM_REG		\
	(OMAP_ABE_NUM_MIXERS + OMAP_ABE_NUM_MUXES + OMAP_ABE_NUM_WIDGETS)

#define OMAP_ABE_ROUTES_UL		14

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

/* TODO: we need the names for each array memeber */
#define OMAP_ABE_IO_RESOURCES	5
#define OMAP_ABE_IO_DMEM 0
#define OMAP_ABE_IO_CMEM 1
#define OMAP_ABE_IO_SMEM 2
#define OMAP_ABE_IO_PMEM 3
#define OMAP_ABE_IO_AESS 4

#define OMAP_ABE_EQU_AMIC	0
#define OMAP_ABE_EQU_DL1	0
#define OMAP_ABE_EQU_DL2L	0
#define OMAP_ABE_EQU_DL2R	0
#define OMAP_ABE_EQU_DMIC	0
#define OMAP_ABE_EQU_SDT	0

/* TODO: combine / sort */
#define OMAP_ABE_MIXER_DEFAULT	128
#define OMAP_ABE_MIXER_MONO	129
#define OMAP_ABE_MIXER_ROUTER	130
#define OMAP_ABE_MIXER_EQU	131
#define OMAP_ABE_MIXER_SWITCH	132
#define OMAP_ABE_MIXER_GAIN	133
#define OMAP_ABE_MIXER_VOLUME	134

#define OMAP_CONTROL_DEFAULT \
	SOC_CONTROL_ID(OMAP_ABE_MIXER_DEFAULT, \
		OMAP_ABE_MIXER_DEFAULT, \
		SOC_CONTROL_TYPE_EXT)
#define OMAP_CONTROL_MONO \
	SOC_CONTROL_ID(OMAP_ABE_MIXER_MONO, \
		OMAP_ABE_MIXER_MONO, \
		SOC_CONTROL_TYPE_VOLSW)
#define OMAP_CONTROL_ROUTER \
	SOC_CONTROL_ID(OMAP_ABE_MIXER_ROUTER, \
		OMAP_ABE_MIXER_ROUTER, \
		SOC_CONTROL_TYPE_ENUM)
#define OMAP_CONTROL_EQU \
	SOC_CONTROL_ID(OMAP_ABE_MIXER_EQU, \
		OMAP_ABE_MIXER_EQU, \
		SOC_CONTROL_TYPE_ENUM)
#define OMAP_CONTROL_SWITCH \
	SOC_CONTROL_ID(OMAP_ABE_MIXER_DEFAULT, \
		OMAP_ABE_MIXER_SWITCH, \
		SOC_CONTROL_TYPE_VOLSW)
#define OMAP_CONTROL_GAIN \
	SOC_CONTROL_ID(OMAP_ABE_MIXER_GAIN, \
		OMAP_ABE_MIXER_GAIN, \
		SOC_CONTROL_TYPE_VOLSW)
#define OMAP_CONTROL_VOLUME \
	SOC_CONTROL_ID(OMAP_ABE_MIXER_VOLUME, \
		OMAP_ABE_MIXER_VOLUME, \
		SOC_CONTROL_TYPE_VOLSW)


#ifdef __KERNEL__

/*
 * ABE Firmware Header.
 * The ABE firmware blob has a header that describes each data section. This
 * way we can store coefficients etc in the firmware.
 */
struct fw_header {
	u32 version;	/* min version of ABE firmware required */
	u32 pmem_size;
	u32 cmem_size;
	u32 dmem_size;
	u32 smem_size;
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
	struct dentry *d_cmem;
	struct dentry *d_pmem;
	struct dentry *d_smem;
	struct dentry *d_dmem;
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

struct omap_abe_coeff {
	int profile; /* current enabled profile */
	int num_profiles;
	int profile_size;
	void *coeff_data;
};

struct omap_abe_equ {
	struct omap_abe_coeff dl1;
	struct omap_abe_coeff dl2l;
	struct omap_abe_coeff dl2r;
	struct omap_abe_coeff sdt;
	struct omap_abe_coeff amic;
	struct omap_abe_coeff dmic;
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
	int (*get_context_lost_count)(struct device *dev);
	int (*device_scale)(struct device *req_dev,
			    struct device *target_dev,
			    unsigned long rate);
	u32 context_lost;

	struct omap_abe_opp opp;
	struct omap_abe_dc_offset dc_offset;
	struct omap_abe_modem modem;
	struct omap_abe_mmap mmap;
	struct omap_abe_equ equ;
	struct omap_abe_dai dai;
	struct omap_abe_mixer mixer;

	/* firmware */
	struct fw_header hdr;
	u32 *fw_config;
	u32 *fw_text;
	const struct firmware *fw;
	int num_equ;

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
void omap_abe_set_dl1_gains(struct snd_soc_platform *platform,
	int left, int right);


extern const struct snd_soc_fw_kcontrol_ops abe_ops[];

#endif  /* __kernel__ */
#endif	/* End of __OMAP_MCPDM_H__ */
