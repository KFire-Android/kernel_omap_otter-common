#ifndef AIC3XXX_CFW_OPS_H_
#define AIC3XXX_CFW_OPS_H_

<<<<<<< HEAD
#ifdef AIC3XXX_CFW_HOST_BLD
    struct mutex {
	    int lock;
    };
#else
#   include <linux/mutex.h>
#   include <sound/soc.h>
#   include <linux/cdev.h>
#endif

typedef struct cfw_project *cfw_project_h;
typedef struct aic3xxx_codec_ops const *aic3xxx_codec_ops_h;
typedef struct cfw_state {
	cfw_project_h pjt;
	aic3xxx_codec_ops_h ops;
=======
#include <linux/mutex.h>
#include <sound/soc.h>
#include <linux/cdev.h>

struct cfw_project;
struct aic3xxx_codec_ops;

struct cfw_state {
	struct cfw_project *pjt;
	struct aic3xxx_codec_ops  *ops;
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
	void *ops_obj;
	struct mutex mutex;
	int cur_mode_id;
	int cur_pll;
	int cur_mode;
	int cur_pfw;
	int cur_ovly;
	int cur_cfg;
<<<<<<< HEAD
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
#   ifdef DEBUG
#      define DBG(fmt, ...) printk("CFW[%s:%d]: " fmt "\n",      \
                           __FILE__, __LINE__, ##__VA_ARGS__)
#   else
#      define DBG(fmt, ...)
#   endif
#endif
int aic3xxx_cfw_init(cfw_state * ps, aic3xxx_codec_ops_h ops,
		     void *ops_obj);
int aic3xxx_cfw_lock(cfw_state * ps, int lock);
int aic3xxx_cfw_reload(cfw_state * ps, void *pcfw, int n);
int aic3xxx_cfw_setmode(cfw_state * ps, int mode);
int aic3xxx_cfw_setmode_cfg(cfw_state * ps, int mode, int cfg);
int aic3xxx_cfw_setcfg(cfw_state * ps, int cfg);
int aic3xxx_cfw_transition(cfw_state * ps, char *ttype);
int aic3xxx_cfw_set_pll(cfw_state * ps, int asi);
int aic3xxx_cfw_control(cfw_state * ps, char *cname, int param);
int aic3xxx_cfw_add_controls(struct snd_soc_codec *codec, cfw_state * ps);
int aic3xxx_cfw_add_modes(struct snd_soc_codec *codec, cfw_state * ps);
=======
	struct cdev cdev;
	int is_open;
};

int aic3xxx_cfw_init(struct cfw_state *ps, struct aic3xxx_codec_ops *ops,
		     void *ops_obj);
int aic3xxx_cfw_lock(struct cfw_state *ps, int lock);
int aic3xxx_cfw_reload(struct cfw_state *ps, void *pcfw, int n);
int aic3xxx_cfw_setmode(struct cfw_state *ps, int mode);
int aic3xxx_cfw_setmode_cfg(struct cfw_state *ps, int mode, int cfg);
int aic3xxx_cfw_setcfg(struct cfw_state *ps, int cfg);
int aic3xxx_cfw_transition(struct cfw_state *ps, char *ttype);
int aic3xxx_cfw_set_pll(struct cfw_state *ps, int asi);
int aic3xxx_cfw_control(struct cfw_state *ps, char *cname, int param);
int aic3xxx_cfw_add_controls(struct snd_soc_codec *codec, struct cfw_state *ps);
int aic3xxx_cfw_add_modes(struct snd_soc_codec *codec, struct cfw_state *ps);
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01


#define AIC3XX_COPS_MDSP_D  (0x00000003u)
#define AIC3XX_COPS_MDSP_A  (0x00000030u)
#define AIC3XX_COPS_MDSP_ALL (AIC3XX_COPS_MDSP_D|AIC3XX_COPS_MDSP_A)

#define AIC3XX_ABUF_MDSP_D1 (0x00000001u)
#define AIC3XX_ABUF_MDSP_D2 (0x00000002u)
#define AIC3XX_ABUF_MDSP_A  (0x00000010u)
#define AIC3XX_ABUF_MDSP_ALL \
<<<<<<< HEAD
    (AIC3XX_ABUF_MDSP_D1| AIC3XX_ABUF_MDSP_D2| AIC3XX_ABUF_MDSP_A)

typedef struct aic3xxx_codec_ops {
=======
		(AIC3XX_ABUF_MDSP_D1|AIC3XX_ABUF_MDSP_D2|AIC3XX_ABUF_MDSP_A)

struct aic3xxx_codec_ops {
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
	int (*reg_read) (void *p, unsigned int reg);
	int (*reg_write) (void *p, unsigned int reg, unsigned int val);
	int (*set_bits) (void *p, unsigned int reg,
			 unsigned char mask, unsigned char val);
<<<<<<< HEAD
	int (*bulk_read) (void *p, unsigned int reg, int count, u8 * buf);
	int (*bulk_write) (void *p, unsigned int reg,
			   int count, const u8 * buf);
=======
	int (*bulk_read) (void *p, unsigned int reg, int count, u8 *buf);
	int (*bulk_write) (void *p, unsigned int reg,
			   int count, const u8 *buf);
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

	int (*lock) (void *p);
	int (*unlock) (void *p);
	int (*stop) (void *p, int mask);
	int (*restore) (void *p, int runstate);
	int (*bswap) (void *p, int mask);
<<<<<<< HEAD
} aic3xxx_codec_ops;
=======
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01


#endif
