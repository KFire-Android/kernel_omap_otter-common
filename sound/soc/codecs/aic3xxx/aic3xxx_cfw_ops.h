
/*
 * aic3xxx_cfw_ops.h  --  SoC audio for TI OMAP44XX SDP
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

#ifndef AIC3XXX_CFW_OPS_H_
#define AIC3XXX_CFW_OPS_H_

#ifdef AIC3XXX_CFW_HOST_BLD
struct mutex {
	int lock;
};
#else
#include <linux/mutex.h>
#include <sound/soc.h>
#include <linux/cdev.h>
#endif

typedef struct cfw_project *cfw_project_h;
typedef struct aic3xxx_codec_ops const *aic3xxx_codec_ops_h;
typedef struct cfw_state {
	cfw_project_h pjt;
	aic3xxx_codec_ops_h ops;
	void *ops_obj;
	struct mutex mutex;
	int cur_mode_id;
	int cur_pll;
	int cur_mode;
	int cur_pfw;
	int cur_ovly;
	int cur_cfg;
#ifndef AIC3XXX_CFW_HOST_BLD
struct cdev cdev;
int is_open;
#endif
} cfw_state;

#ifdef AIC3XXX_CFW_HOST_BLD
cfw_project *aic3xxx_cfw_unpickle(void *pcfw, int n);
unsigned int crc32(unsigned int *pdata, int n);
struct snd_soc_codec;
#else
#ifdef DEBUG
#define DBG(fmt, ...) printk("CFW[%s:%d]: " fmt "\n", \
				__FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DBG(fmt, ...)
#endif
#endif
int aic3xxx_cfw_init(cfw_state *ps, aic3xxx_codec_ops_h ops,
			void *ops_obj);
int aic3xxx_cfw_lock(cfw_state *ps, int lock);
int aic3xxx_cfw_reload(cfw_state *ps, void *pcfw, int n);
int aic3xxx_cfw_setmode(cfw_state *ps, int mode);
int aic3xxx_cfw_setmode_cfg(cfw_state *ps, int mode, int cfg);
int aic3xxx_cfw_setcfg(cfw_state *ps, int cfg);
int aic3xxx_cfw_transition(cfw_state *ps, char *ttype);
int aic3xxx_cfw_set_pll(cfw_state *ps, int asi);
int aic3xxx_cfw_control(cfw_state *ps, char *cname, int param);
int aic3xxx_cfw_add_controls(struct snd_soc_codec *codec, cfw_state *ps);
int aic3xxx_cfw_add_modes(struct snd_soc_codec *codec, cfw_state *ps);


#define AIC3XX_COPS_MDSP_D	(0x00000003u)
#define AIC3XX_COPS_MDSP_A	(0x00000030u)
#define AIC3XX_COPS_MDSP_ALL	(AIC3XX_COPS_MDSP_D|AIC3XX_COPS_MDSP_A)

#define AIC3XX_ABUF_MDSP_D1	(0x00000001u)
#define AIC3XX_ABUF_MDSP_D2	(0x00000002u)
#define AIC3XX_ABUF_MDSP_A  (0x00000010u)
#define AIC3XX_ABUF_MDSP_ALL \
	(AIC3XX_ABUF_MDSP_D1 | AIC3XX_ABUF_MDSP_D2 | AIC3XX_ABUF_MDSP_A)

typedef struct aic3xxx_codec_ops {
	int (*reg_read) (void *p, unsigned int reg);
	int (*reg_write) (void *p, unsigned int reg, unsigned char val);
	int (*set_bits) (void *p, unsigned int reg,
			 unsigned char mask, unsigned char val);
	int (*bulk_read) (void *p, unsigned int reg, int count, u8 *buf);
	int (*bulk_write) (void *p, unsigned int reg,
			   int count, const u8 *buf);

	int (*lock) (void *p);
	int (*unlock) (void *p);
	int (*stop) (void *p, int mask);
	int (*restore) (void *p, int runstate);
	int (*bswap) (void *p, int mask);
} aic3xxx_codec_ops;

MODULE_LICENSE("GPL");

#endif
