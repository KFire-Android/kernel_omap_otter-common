/**
 * \file Codec Firmware Declarations
 */

#ifndef CFW_FIRMWARE_H_
#define CFW_FIRMWARE_H_

<<<<<<< HEAD
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
=======

#define CFW_FW_MAGIC 0xC0D1F1ED
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01


/** \defgroup pd Arbitrary Limitations */
/* @{ */
#ifndef CFW_MAX_ID
<<<<<<< HEAD
#    define CFW_MAX_ID          (64)	///<Max length of string identifies
#    define CFW_MAX_VARS       (256)	///<Number of "variables" alive at the
					///<same time in an acx file
=======
#    define CFW_MAX_ID          (64)	/**<Max length of string identifies*/
#    define CFW_MAX_VARS       (256)	/**<Number of "variables" alive at the*/
					/**<same time in an acx file*/
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
#endif

/* @} */



/** \defgroup st Enums, Flags, Macros and Supporting Types */
/* @{ */


/**
 * Device Family Identifier
 *
 */
<<<<<<< HEAD
typedef enum __attribute__ ((__packed__)) cfw_dfamily {
	CFW_DFM_TYPE_A,
	CFW_DFM_TYPE_B,
	CFW_DFM_TYPE_C
} cfw_dfamily;
=======
enum __attribute__ ((__packed__)) cfw_dfamily {
	CFW_DFM_TYPE_A,
	CFW_DFM_TYPE_B,
	CFW_DFM_TYPE_C
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * Device Identifier
 *
 */
<<<<<<< HEAD
typedef enum __attribute__ ((__packed__)) cfw_device {
=======
enum __attribute__ ((__packed__)) cfw_device {
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
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
<<<<<<< HEAD
} cfw_device;
=======
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * Transition Sequence Identifier
 *
 */
<<<<<<< HEAD
typedef enum cfw_transition_t {
=======
enum cfw_transition_t {
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
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
<<<<<<< HEAD
} cfw_transition_t;
=======
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

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
<<<<<<< HEAD
typedef enum __attribute__ ((__packed__)) cfw_cmd_id {
=======
enum __attribute__ ((__packed__)) cfw_cmd_id {
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
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
<<<<<<< HEAD
} cfw_cmd_id;
=======
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
* CFW Delay
* Used for the cmd command delay
* Has one parameter of delay time in ms
*/
<<<<<<< HEAD
typedef struct cfw_cmd_delay {
	u16 delay;
	cfw_cmd_id cid;
	u8 delay_fine;
} cfw_cmd_delay;
=======
struct cfw_cmd_delay {
	u16 delay;
	enum cfw_cmd_id cid;
	u8 delay_fine;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
* CFW Lock
* Take codec mutex to avoid clashing with DAPM operations
*/
<<<<<<< HEAD
typedef struct cfw_cmd_lock {
	u16 lock;
	cfw_cmd_id cid;
	u8 unused;
} cfw_cmd_lock;
=======
struct cfw_cmd_lock {
	u16 lock;
	enum cfw_cmd_id cid;
	u8 unused;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01


/**
 * CFW  UPDTBITS, WAITBITS, CHKBITS
 * Both these cmd commands have same arguments
 * cid will be used to specify which command it is
 * has parameters of book, page, offset and mask
 */
<<<<<<< HEAD
typedef struct cfw_cmd_bitop {
	u16 unused1;
	cfw_cmd_id cid;
	u8 mask;
} cfw_cmd_bitop;
=======
struct cfw_cmd_bitop {
	u16 unused1;
	enum cfw_cmd_id cid;
	u8 mask;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * CFW  CMD Burst header
 * Burst writes inside command array
 * Followed by burst address, first byte
 */
<<<<<<< HEAD
typedef struct cfw_cmd_bhdr {
	u16 len;
	cfw_cmd_id cid;
	u8 unused;
} cfw_cmd_bhdr;
=======
struct cfw_cmd_bhdr {
	u16 len;
	enum cfw_cmd_id cid;
	u8 unused;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * CFW  CMD Burst
 * Burst writes inside command array
 * Followed by data to the extent indicated in previous len
 * Can be safely cast to cfw_burst
 */
<<<<<<< HEAD
typedef struct cfw_cmd_burst {
=======
struct cfw_cmd_burst {
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
	u8 book;
	u8 page;
	u8 offset;
	u8 data[1];
<<<<<<< HEAD
} cfw_cmd_burst;
=======
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
#define CFW_CMD_BURST_LEN(n) (2 + ((n) - 1 + 3)/4)

/**
 * CFW  CMD Scratch register
 * For load
 *  if (svar != dvar)
 *      dvar = setbits(svar, mask) // Ignore reg
 *  else
 *      dvar = setbits(reg, mask)
 * For store
 *  if (svar != dvar)
 *      reg = setbits(svar,  dvar)
 *  else
 *      reg =  setbits(svar, mask)
 *
 */
<<<<<<< HEAD
typedef struct cfw_cmd_ldst {
	u8 dvar;
	u8 svar;
	cfw_cmd_id cid;
	u8 mask;
} cfw_cmd_ldst;
=======
struct cfw_cmd_ldst {
	u8 dvar;
	u8 svar;
	enum cfw_cmd_id cid;
	u8 mask;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * CFW  CMD Conditional
 * May only precede branch. Followed by nmatch+1 jump
 * instructions
 *   cond = svar&mask
 * At each of the following nmatch+1 branch command
 *   if (cond == match)
 *       take the branch
 */
<<<<<<< HEAD
typedef struct cfw_cmd_cond {
	u8 svar;
	u8 nmatch;
	cfw_cmd_id cid;
	u8 mask;
} cfw_cmd_cond;
=======
struct cfw_cmd_cond {
	u8 svar;
	u8 nmatch;
	enum cfw_cmd_id cid;
	u8 mask;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
#define CFW_CMD_COND_LEN(nm) (1 + ((nm)+1))

/**
 * CFW  CMD Goto
 * For branch, break, continue and stop
 */
<<<<<<< HEAD
typedef struct cfw_cmd_branch {
	u16 address;
	cfw_cmd_id cid;
	u8 match;
} cfw_cmd_branch;
=======
struct cfw_cmd_branch {
	u16 address;
	enum cfw_cmd_id cid;
	u8 match;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * CFW  Debug print
 * For diagnostics
 */
<<<<<<< HEAD
typedef struct cfw_cmd_print {
	u8 fmtlen;
	u8 nargs;
	cfw_cmd_id cid;
	char fmt[1];
} cfw_cmd_print;

typedef u8 cfw_cmd_print_arg[4];
=======
struct cfw_cmd_print {
	u8 fmtlen;
	u8 nargs;
	enum cfw_cmd_id cid;
	char fmt[1];
};

>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
#define CFW_CMD_PRINT_LEN(p) (1 + ((p).fmtlen/4) + (((p).nargs + 3)/4))
#define CFW_CMD_PRINT_ARG(p) (1 + ((p).fmtlen/4))

/**
 * CFW  Arithmetic and logical operations
 *  Bit 5 indicates if op1 is indirect
 *  Bit 6 indicates if op2 is indirect
 */
<<<<<<< HEAD
typedef struct cfw_cmd_op {
	u8 op1;
	u8 op2;
	cfw_cmd_id cid;
	u8 dst;
} cfw_cmd_op;
=======
struct cfw_cmd_op {
	u8 op1;
	u8 op2;
	enum cfw_cmd_id cid;
	u8 dst;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
#define CFW_CMD_OP1_ID     (1u<<5)
#define CFW_CMD_OP2_ID     (1u<<4)

#define CFW_CMD_OP_START   CFW_CMD_OP_ADD
#define CFW_CMD_OP_END     (CFW_CMD_OP_LNOT|CFW_CMD_OP1_ID|CFW_CMD_OP2_ID)
#define CFW_CMD_OP_IS_UNARY(x) \
<<<<<<< HEAD
    (((x) == CFW_CMD_OP_NOT) || ((x) == CFW_CMD_OP_LNOT))
=======
			(((x) == CFW_CMD_OP_NOT) || ((x) == CFW_CMD_OP_LNOT))
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01


/**
 * CFW Register
 *
 * A single reg write
 *
 */
<<<<<<< HEAD
typedef union cfw_register {
=======
union cfw_register {
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
	struct {
		u8 book;
		u8 page;
		u8 offset;
		u8 data;
	};
	u32 bpod;
<<<<<<< HEAD
} cfw_register;
=======
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01



/**
 * CFW Command
 *
 * Can be a either a
 *      -# single register write, or
 *      -# command
 *
 */
<<<<<<< HEAD
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
=======
union cfw_cmd {
	struct {
		u16 unused1;
		enum cfw_cmd_id cid;
		u8 unused2;
	};
	union cfw_register reg;
	struct cfw_cmd_delay delay;
	struct cfw_cmd_lock lock;
	struct cfw_cmd_bitop bitop;
	struct cfw_cmd_bhdr bhdr;
	struct cfw_cmd_burst burst;
	struct cfw_cmd_ldst ldst;
	struct cfw_cmd_cond cond;
	struct cfw_cmd_branch branch;
	struct cfw_cmd_print print;
	u8     print_arg[4];
	struct cfw_cmd_op op;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

#define CFW_REG_IS_CMD(x) ((x).cid >= CFW_CMD_DELAY)

/**
 * CFW Block Type
 *
 * Block identifier
 *
 */
<<<<<<< HEAD
typedef enum __attribute__ ((__packed__)) cfw_block_t {
=======
enum __attribute__ ((__packed__)) cfw_block_t {
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
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
<<<<<<< HEAD
} cfw_block_t;
=======
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
#define CFW_BLOCK_D_A_COEF CFW_BLOCK_D_A1_COEF
#define CFW_BLOCK_D_B_COEF CFW_BLOCK_D_B1_COEF

/**
 * CFW Block
 *
 * A block of logically grouped sequences/commands/cmd-commands
 *
 */
<<<<<<< HEAD
typedef struct cfw_block {
	cfw_block_t type;
	int ncmds;
	cfw_cmd cmd[];
} cfw_block;

=======
struct cfw_block {
	enum cfw_block_t type;
	int ncmds;
	union cfw_cmd cmd[];
};
#define CFW_BLOCK_SIZE(ncmds) (sizeof(struct cfw_block) + \
				((ncmds)*sizeof(union cfw_cmd)))
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * CFW Image
 *
 * A downloadable image
 */
<<<<<<< HEAD
typedef struct cfw_image {
	char name[CFW_MAX_ID];	///< Name of the pfw/overlay/configuration
	char *desc;		///< User string
	int mute_flags;
	cfw_block *block[CFW_BLOCK_N];
} cfw_image;
=======
struct cfw_image {
	char name[CFW_MAX_ID];	/**< Name of the pfw/overlay/configuration*/
	char *desc;		/**< User string*/
	int mute_flags;
	struct cfw_block *block[CFW_BLOCK_N];
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01



/**
 * CFW PLL
 *
 * PLL configuration sequence and match critirea
 */
<<<<<<< HEAD
typedef struct cfw_pll {
	char name[CFW_MAX_ID];	///< Name of the PLL sequence
	char *desc;		///< User string
	cfw_block *seq;
} cfw_pll;
=======
struct cfw_pll {
	char name[CFW_MAX_ID];	/**< Name of the PLL sequence*/
	char *desc;		/**< User string*/
	struct cfw_block *seq;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * CFW Control
 *
 * Run-time control for a process flow
 */
<<<<<<< HEAD
typedef struct cfw_control {
	char name[CFW_MAX_ID];	///< Control identifier
	char *desc;		///< User string
	int mute_flags;

	int min;		///< Min value of control (*100)
	int max;		///< Max  value of control (*100)
	int step;		///< Control step size (*100)

	int imax;		///< Max index into controls array
	int ireset;		///< Reset control to defaults
	int icur;		///< Last value set
	cfw_block **output;	///< Array of sequences to send
} cfw_control;
=======
struct cfw_control {
	char name[CFW_MAX_ID];	/**< Control identifier*/
	char *desc;		/**< User string*/
	int mute_flags;

	int min;		/**< Min value of control (*100)*/
	int max;		/**< Max  value of control (*100)*/
	int step;		/**< Control step size (*100)*/

	int imax;		/**< Max index into controls array*/
	int ireset;		/**< Reset control to defaults*/
	int icur;		/**< Last value set*/
	struct cfw_block **output;	/**< Array of sequences to send*/
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * Process flow
 *
 * Complete description of a process flow
 */
<<<<<<< HEAD
typedef struct cfw_pfw {
	char name[CFW_MAX_ID];	///< Name of the process flow
	char *desc;		///< User string
	u32 version;
	u8 prb_a;
	u8 prb_d;
	int novly;		///< Number of overlays (1 or more)
	int ncfg;		///< Number of configurations (0 or more)
	int nctrl;		///< Number of run-time controls
	cfw_image *base;	///< Base sequence
	cfw_image **ovly_cfg;	///< Overlay and cfg
				///< patches (if any)
	cfw_control **ctrl;	///< Array of run-time controls
} cfw_pfw;
=======
struct cfw_pfw {
	char name[CFW_MAX_ID];	/**< Name of the process flow*/
	char *desc;		/**< User string*/
	u32 version;
	u8 prb_a;
	u8 prb_d;
	int novly;		/**< Number of overlays (1 or more)*/
	int ncfg;		/**< Number of configurations (0 or more)*/
	int nctrl;		/**< Number of run-time controls*/
	struct cfw_image *base;	/**< Base sequence*/
	struct cfw_image **ovly_cfg;	/**< Overlay and cfg*/
					/**< patches (if any)*/
	struct cfw_control **ctrl;	/**< Array of run-time controls*/
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

#define CFW_OCFG_NDX(p, o, c) (((o)*(p)->ncfg)+(c))
/**
 * Process transition
 *
 * Sequence for specific state transisitions within the driver
 *
 */
<<<<<<< HEAD
typedef struct cfw_transition {
	char name[CFW_MAX_ID];	///< Name of the transition
	char *desc;		///< User string
	cfw_block *block;
} cfw_transition;
=======
struct cfw_transition {
	char name[CFW_MAX_ID];	/**< Name of the transition*/
	char *desc;		/**< User string*/
	struct cfw_block *block;
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * Device audio mode
 *
 * Link operating modes to process flows,
 * configurations and sequences
 *
 */
<<<<<<< HEAD
typedef struct cfw_mode {
	char name[CFW_MAX_ID];
	char *desc;		///< User string
=======
struct cfw_mode {
	char name[CFW_MAX_ID];
	char *desc;		/**< User string*/
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01
	u32 flags;
	u8 pfw;
	u8 ovly;
	u8 cfg;
	u8 pll;
<<<<<<< HEAD
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
=======
	struct cfw_block *entry;
	struct cfw_block *exit;
};

struct cfw_asoc_toc_entry {
	char etext[CFW_MAX_ID];
	int mode;
	int cfg;
};

struct cfw_asoc_toc {
	int nentries;
	struct cfw_asoc_toc_entry entry[];
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01

/**
 * CFW Project
 *
 * Top level structure describing the CFW project
 */
<<<<<<< HEAD
typedef struct cfw_project {
	u32 magic;		///< magic number for identifying F/W file
	u32 if_id;		///< Interface match code (changed on every structure change)
	u32 size;		///< Total size of the firmware (including this header)
	u32 cksum;		///< CRC32 of the pickled firmware (with this field set to zero)
	u32 version;		///< Firmware version (from CFD file)
	u32 tstamp;		///< Time stamp of firmware build (epoch seconds)
	char name[CFW_MAX_ID];	///< Project name
	char *desc;		///< User string
	cfw_dfamily dfamily;	///< Device family
	cfw_device device;	///< Device identifier
	u32 flags;		///< CFW flags

	cfw_transition **transition;	///< Transition sequences

	u16 npll;		///< Number of PLL settings
	cfw_pll **pll;		///< PLL settings

	u16 npfw;		///< Number of process flows
	cfw_pfw **pfw;		///< Process flows

	u16 nmode;		///< Number of operating modes
	cfw_mode **mode;	///< Modes

	cfw_asoc_toc *asoc_toc;	///< table of contents of mixer controls
} cfw_project;
=======
struct cfw_project {
	u32 magic;		/**< magic number for identifying F/W file*/
	u32 if_id;		/**< Interface match code */
	u32 size;		/**< Total size of the firmware (including this header)*/
	u32 cksum;		/**< CRC32 of the pickled firmware */
	u32 version;		/**< Firmware version (from CFD file)*/
	u32 tstamp;		/**< Time stamp of firmware build (epoch seconds)*/
	char name[CFW_MAX_ID];	/**< Project name*/
	char *desc;		/**< User string*/
	enum cfw_dfamily dfamily;	/**< Device family*/
	enum cfw_device device;	/**< Device identifier*/
	u32 flags;		/**< CFW flags*/

	struct cfw_transition **transition;	/**< Transition sequences*/

	u16 npll;		/**< Number of PLL settings*/
	struct cfw_pll **pll;	/**< PLL settings*/

	u16 npfw;		/**< Number of process flows*/
	struct cfw_pfw **pfw;	/**< Process flows*/

	u16 nmode;		/**< Number of operating modes*/
	struct cfw_mode **mode;	/**< Modes*/

	struct cfw_asoc_toc *asoc_toc;	/**< list of amixer controls*/
};
>>>>>>> 4864a7a7756b952ed04d1809312bf8331c7a5a01


/* @} */

/* **CFW_INTERFACE_ID=0x3FA6D547** */

#endif				/* CFW_FIRMWARE_H_ */
