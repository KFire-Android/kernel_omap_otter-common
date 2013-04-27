/*
 *  aic3xxx_cfw.h  --  SoC audio for TI OMAP44XX SDP
 *                      Codec Firmware Declarations
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
/**
 * \file Codec Firmware Declarations
 */

#ifndef CFW_FIRMWARE_H_
#define CFW_FIRMWARE_H_

/** \defgroup bt Basic Types */
/* @{ */
#ifndef AIC3XXX_CFW_HOST_BLD
#include <asm-generic/int-ll64.h>
#else
typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned long int u32;
#endif
typedef signed char i8;
typedef signed short int i16;
typedef signed long int i32;

#define CFW_FW_MAGIC 0xC0D1F1ED
/* @} */


/** \defgroup pd Arbitrary Limitations */
/* @{ */
#ifndef CFW_MAX_ID
#    define CFW_MAX_ID          (64)	/* /<Max length of string identifies */
#    define CFW_MAX_VARS       (256)	/* /<Number of "variables" alive at the */
					/* /<same time in an acx file */
#endif

/* @} */



/** \defgroup st Enums, Flags, Macros and Supporting Types */
/* @{ */


/**
 * Device Family Identifier
 *
 */
typedef enum __attribute__ ((__packed__)) cfw_dfamily {
	CFW_DFM_TYPE_A,
	CFW_DFM_TYPE_B,
	CFW_DFM_TYPE_C
} cfw_dfamily;

/**
 * Device Identifier
 *
 */
typedef enum __attribute__ ((__packed__)) cfw_device {
	CFW_DEV_DAC3120,
	CFW_DEV_DAC3100,
	CFW_DEV_AIC3120,
	CFW_DEV_AIC3100,
	CFW_DEV_AIC3110,
	CFW_DEV_AIC3111,
	CFW_DEV_AIC36,
	CFW_DEV_AIC3206,
	CFW_DEV_AIC3204,
	CFW_DEV_AIC3254,
	CFW_DEV_AIC3256,
	CFW_DEV_AIC3253,
	CFW_DEV_AIC3212,
	CFW_DEV_AIC3262,
	CFW_DEV_AIC3017,
	CFW_DEV_AIC3008,

	CFW_DEV_AIC3266,
	CFW_DEV_AIC3285,
} cfw_device;

/**
 * Transition Sequence Identifier
 *
 */
typedef enum cfw_transition_t {
	CFW_TRN_INIT,
	CFW_TRN_RESUME,
	CFW_TRN_NEUTRAL,
	CFW_TRN_A_MUTE,
	CFW_TRN_D_MUTE,
	CFW_TRN_AD_MUTE,
	CFW_TRN_A_UNMUTE,
	CFW_TRN_D_UNMUTE,
	CFW_TRN_AD_UNMUTE,
	CFW_TRN_SUSPEND,
	CFW_TRN_EXIT,
	CFW_TRN_N
} cfw_transition_t;

#ifndef __cplusplus
static const char *const cfw_transition_id[] = {
	[CFW_TRN_INIT]     "INIT",
	[CFW_TRN_RESUME]   "RESUME",
	[CFW_TRN_NEUTRAL]  "NEUTRAL",
	[CFW_TRN_A_MUTE]   "A_MUTE",
	[CFW_TRN_D_MUTE]   "D_MUTE",
	[CFW_TRN_AD_MUTE]  "AD_MUTE",
	[CFW_TRN_A_UNMUTE] "A_UNMUTE",
	[CFW_TRN_D_UNMUTE] "D_UNMUTE",
	[CFW_TRN_AD_UNMUTE]"AD_UNMUTE",
	[CFW_TRN_SUSPEND]  "SUSPEND",
	[CFW_TRN_EXIT]     "EXIT",
};
#endif

/* @} */

/** \defgroup ds Data Structures */
/* @{ */


/**
* CFW Command
* These commands do not appear in the register
* set of the device.
*/
typedef enum __attribute__ ((__packed__)) cfw_cmd_id {
	CFW_CMD_NOP = 0x80,
	CFW_CMD_DELAY,
	CFW_CMD_UPDTBITS,
	CFW_CMD_WAITBITS,
	CFW_CMD_LOCK,
	CFW_CMD_BURST,
	CFW_CMD_RBURST,
	CFW_CMD_LOAD_VAR_IM,
	CFW_CMD_LOAD_VAR_ID,
	CFW_CMD_STORE_VAR,
	CFW_CMD_COND,
	CFW_CMD_BRANCH,
	CFW_CMD_BRANCH_IM,
	CFW_CMD_BRANCH_ID,
	CFW_CMD_PRINT,
	CFW_CMD_OP_ADD = 0xC0,
	CFW_CMD_OP_SUB,
	CFW_CMD_OP_MUL,
	CFW_CMD_OP_DIV,
	CFW_CMD_OP_AND,
	CFW_CMD_OP_OR,
	CFW_CMD_OP_SHL,
	CFW_CMD_OP_SHR,
	CFW_CMD_OP_RR,
	CFW_CMD_OP_XOR,
	CFW_CMD_OP_NOT,
	CFW_CMD_OP_LNOT,
} cfw_cmd_id;

/**
* CFW Delay
* Used for the cmd command delay
* Has one parameter of delay time in ms
*/
typedef struct cfw_cmd_delay {
	u16 delay;
	cfw_cmd_id cid;
	u8 delay_fine;
} cfw_cmd_delay;

/**
* CFW Lock
* Take codec mutex to avoid clashing with DAPM operations
*/
typedef struct cfw_cmd_lock {
	u16 lock;
	cfw_cmd_id cid;
	u8 unused;
} cfw_cmd_lock;


/**
 * CFW  UPDTBITS, WAITBITS, CHKBITS
 * Both these cmd commands have same arguments
 * cid will be used to specify which command it is
 * has parameters of book, page, offset and mask
 */
typedef struct cfw_cmd_bitop {
	u16 unused1;
	cfw_cmd_id cid;
	u8 mask;
} cfw_cmd_bitop;

/**
 * CFW  CMD Burst header
 * Burst writes inside command array
 * Followed by burst address, first byte
 */
typedef struct cfw_cmd_bhdr {
	u16 len;
	cfw_cmd_id cid;
	u8 unused;
} cfw_cmd_bhdr;

/**
 * CFW  CMD Burst
 * Burst writes inside command array
 * Followed by data to the extent indicated in previous len
 * Can be safely cast to cfw_burst
 */
typedef struct cfw_cmd_burst {
	u8 book;
	u8 page;
	u8 offset;
	u8 data[1];
} cfw_cmd_burst;
#define CFW_CMD_BURST_LEN(n) (2 + ((n) - 1 + 3)/4)

/**
 * CFW  CMD Scratch register
 * For load
 *  if (svar != dvar)
 *      dvar = setbits(svar, mask) *  Ignore reg *
 *  else
 *      dvar = setbits(reg, mask)
 * For store
 *  if (svar != dvar)
 *      reg = setbits(svar,  dvar)
 *  else
 *      reg =  setbits(svar, mask)
 *
 */
typedef struct cfw_cmd_ldst {
	u8 dvar;
	u8 svar;
	cfw_cmd_id cid;
	u8 mask;
} cfw_cmd_ldst;

/**
 * CFW  CMD Conditional
 * May only precede branch. Followed by nmatch+1 jump
 * instructions
 * cond = svar&mask
 * At each of the following nmatch+1 branch command
 * if (cond == match)
 * take the branch
 */
typedef struct cfw_cmd_cond {
	u8 svar;
	u8 nmatch;
	cfw_cmd_id cid;
	u8 mask;
} cfw_cmd_cond;
#define CFW_CMD_COND_LEN(nm) (1 + ((nm)+1))

/**
 * CFW  CMD Goto
 * For branch, break, continue and stop
 */
typedef struct cfw_cmd_branch {
	u16 address;
	cfw_cmd_id cid;
	u8 match;
} cfw_cmd_branch;

/**
 * CFW  Debug print
 * For diagnostics
 */
typedef struct cfw_cmd_print {
	u8 fmtlen;
	u8 nargs;
	cfw_cmd_id cid;
	char fmt[1];
} cfw_cmd_print;

typedef u8 cfw_cmd_print_arg[4];
#define CFW_CMD_PRINT_LEN(p) (1 + ((p).fmtlen/4) + (((p).nargs + 3)/4))
#define CFW_CMD_PRINT_ARG(p) (1 + ((p).fmtlen/4))

/**
 * CFW  Arithmetic and logical operations
 *  Bit 5 indicates if op1 is indirect
 *  Bit 6 indicates if op2 is indirect
 */
typedef struct cfw_cmd_op {
	u8 op1;
	u8 op2;
	cfw_cmd_id cid;
	u8 dst;
} cfw_cmd_op;
#define CFW_CMD_OP1_ID	(1u<<5)
#define CFW_CMD_OP2_ID	(1u<<4)

#define CFW_CMD_OP_START	CFW_CMD_OP_ADD
#define CFW_CMD_OP_END	(CFW_CMD_OP_LNOT|CFW_CMD_OP1_ID|CFW_CMD_OP2_ID)
#define CFW_CMD_OP_IS_UNARY(x) \
	(((x) == CFW_CMD_OP_NOT) || ((x) == CFW_CMD_OP_LNOT))


/**
 * CFW Register
 *
 * A single reg write
 *
 */
typedef union cfw_register {
	struct {
		u8 book;
		u8 page;
		u8 offset;
		u8 data;
	};
	u32 bpod;
} cfw_register;



/**
 * CFW Command
 *
 * Can be a either a
 * -# single register write, or
 * -# command
 *
 */
typedef union cfw_cmd {
	struct {
		u16 unused1;
		cfw_cmd_id cid;
		u8 unused2;
	};
	cfw_register reg;
	cfw_cmd_delay delay;
	cfw_cmd_lock lock;
	cfw_cmd_bitop bitop;
	cfw_cmd_bhdr bhdr;
	cfw_cmd_burst burst;
	cfw_cmd_ldst ldst;
	cfw_cmd_cond cond;
	cfw_cmd_branch branch;
	cfw_cmd_print print;
	cfw_cmd_print_arg print_arg;
	cfw_cmd_op op;
} cfw_cmd;

#define CFW_REG_IS_CMD(x) ((x).cid >= CFW_CMD_DELAY)

/**
 * CFW Block Type
 *
 * Block identifier
 *
 */
typedef enum __attribute__ ((__packed__)) cfw_block_t {
	CFW_BLOCK_SYSTEM_PRE,
	CFW_BLOCK_A_INST,
	CFW_BLOCK_A_A_COEF,
	CFW_BLOCK_A_B_COEF,
	CFW_BLOCK_A_F_COEF,
	CFW_BLOCK_D_INST,
	CFW_BLOCK_D_A1_COEF,
	CFW_BLOCK_D_B1_COEF,
	CFW_BLOCK_D_A2_COEF,
	CFW_BLOCK_D_B2_COEF,
	CFW_BLOCK_D_F_COEF,
	CFW_BLOCK_SYSTEM_POST,
	CFW_BLOCK_N,
	CFW_BLOCK_INVALID,
} cfw_block_t;
#define CFW_BLOCK_D_A_COEF CFW_BLOCK_D_A1_COEF
#define CFW_BLOCK_D_B_COEF CFW_BLOCK_D_B1_COEF

/**
 * CFW Block
 *
 * A block of logically grouped sequences/commands/cmd-commands
 *
 */
typedef struct cfw_block {
	cfw_block_t type;
	int ncmds;
	cfw_cmd cmd[];
} cfw_block;


/**
 * CFW Image
 *
 * A downloadable image
 */
typedef struct cfw_image {
	char name[CFW_MAX_ID];	/*Name of the pfw/overlay/configuration */
	char *desc;		/* User string */
	int mute_flags;
	cfw_block * block[CFW_BLOCK_N];
} cfw_image;



/**
 * CFW PLL
 *
 * PLL configuration sequence and match critirea
 */
typedef struct cfw_pll {
	char name[CFW_MAX_ID];	/* /< Name of the PLL sequence */
	char *desc;		/* /< User string */
	cfw_block *seq;
} cfw_pll;

/**
 * CFW Control
 *
 * Run-time control for a process flow
 */
typedef struct cfw_control {
	char name[CFW_MAX_ID];	/* /< Control identifier */
	char *desc;		/* /< User string */
	int mute_flags;

	int min;		/* /< Min value of control (*100) */
	int max;		/* /< Max  value of control (*100) */
	int step;		/* /< Control step size (*100) */

	int imax;		/* /< Max index into controls array */
	int ireset;		/* /< Reset control to defaults */
	int icur;		/* /< Last value set */
	cfw_block **output;	/* /< Array of sequences to send */
} cfw_control;

/**
 * Process flow
 *
 * Complete description of a process flow
 */
typedef struct cfw_pfw {
	char name[CFW_MAX_ID];	/* /< Name of the process flow */
	char *desc;		/* /< User string */
	u32 version;
	u8 prb_a;
	u8 prb_d;
	int novly;		/* /< Number of overlays (1 or more) */
	int ncfg;		/* /< Number of configurations (0 or more) */
	int nctrl;		/* /< Number of run-time controls */
	cfw_image *base;	/* /< Base sequence */
	cfw_image **ovly_cfg;	/* /< Overlay and cfg */
				/* /< patches (if any) */
	cfw_control **ctrl;	/* /< Array of run-time controls */
} cfw_pfw;

#define CFW_OCFG_NDX(p, o, c) (((o)*(p)->ncfg)+(c))
/**
 * Process transition
 *
 * Sequence for specific state transisitions within the driver
 *
 */
typedef struct cfw_transition {
	char name[CFW_MAX_ID];	/* /< Name of the transition */
	char *desc;		/* /< User string */
	cfw_block *block;
} cfw_transition;

/**
 * Device audio mode
 *
 * Link operating modes to process flows,
 * configurations and sequences
 *
 */
typedef struct cfw_mode {
	char name[CFW_MAX_ID];
	char *desc;		/* /< User string */
	u32 flags;
	u8 pfw;
	u8 ovly;
	u8 cfg;
	u8 pll;
	cfw_block *entry;
	cfw_block *exit;
} cfw_mode;

typedef struct cfw_asoc_toc_entry {
	char etext[CFW_MAX_ID];
	int mode;
	int cfg;
} cfw_asoc_toc_entry;

typedef struct cfw_asoc_toc {
	int nentries;
	cfw_asoc_toc_entry entry[];
} cfw_asoc_toc;

/**
 * CFW Project
 *
 * Top level structure describing the CFW project
 */
typedef struct cfw_project {
	u32 magic;		/*magic number for identifying F/W file */
	u32 if_id;/*Interface match code (changed on every structure change)*/
	u32 size;		/*Total size of the firmware (including this header) */
	u32 cksum;/*CRC32 ofthe pickled firmware (with this field set to zero)*/
	u32 version;		/*Firmware version (from CFD file) */
	u32 tstamp;		/*Time stamp of firmware build (epoch seconds) */
	char name[CFW_MAX_ID];	/*Project name */
	char *desc;		/*User string */
	cfw_dfamily dfamily;	/*Device family */
	cfw_device device;	/*Device identifier */
	u32 flags;		/*CFW flags */

	cfw_transition **transition;	/*Transition sequences */

	u16 npll;		/*Number of PLL settings */
	cfw_pll **pll;		/*PLL settings */

	u16 npfw;		/*Number of process flows */
	cfw_pfw **pfw;		/*Process flows */

	u16 nmode;		/*Number of operating modes */
	cfw_mode **mode;	/*Modes */

	cfw_asoc_toc *asoc_toc;	/*table of contents of mixer controls */
} cfw_project;


/* @} */

/* **CFW_INTERFACE_ID=0x3FA6D547** */

#endif				/* CFW_FIRMWARE_H_ */
