#ifndef _DRA7XX_VIP_H_
#define _DRA7XX_VIP_H_

#include <linux/ioctl.h>
#include <linux/videodev2.h>

/*
 * ioctl definitions for the OMAP52xx VIP (Video Processing Engine)
 */

struct vip_csc_coeffs {
	unsigned short	a0;
	unsigned short	b0;
	unsigned short	c0;
	unsigned short	a1;
	unsigned short	b1;
	unsigned short	c1;
	unsigned short	a2;
	unsigned short	b2;
	unsigned short	c2;
	unsigned short	d0;
	unsigned short	d1;
	unsigned short	d2;
};

/*
 * Set/Get the custom array of Color Space Conversion coefficients
 * for conversion from YUV to RGB.
 */
struct vip_colorspace_coeffs {
	struct vip_csc_coeffs	sd;
	struct vip_csc_coeffs	hd;
};

/*
 * Set/Get which set of Color Space Conversion coefficients to use.
 */
typedef enum {
	VIP_CSC_COEFFS_LIMITED_RANGE_Y2R = 0,
	VIP_CSC_COEFFS_FULL_RANGE_Y2R,
	VIP_CSC_COEFFS_LIMITED_RANGE_R2Y,
	VIP_CSC_COEFFS_FULL_RANGE_R2Y,
	VIP_CSC_COEFFS_CUSTOM,
} vip_csc_coeffs_sel;

/*
 * Set/Get the custom array of Scaler Peaking coefficients.
 */
struct vip_sc_peaking {
	unsigned char	hpf_coeff[6];
	unsigned char	hpf_norm_shift;
	unsigned short	nl_clip_limit;
	unsigned short	nl_low_thr;
	unsigned short	nl_high_thr;
	unsigned short	nl_low_slope_gain;
	unsigned short	nl_high_slope_shift;
};

/*
 * Set/Get which set of Scaler Peaking coefficients to use.
 */
typedef enum {
	VIP_SC_PEAKING_NONE = 0,
	VIP_SC_PEAKING_3_3_MHz,
	VIP_SC_PEAKING_4_6_MHz,
	VIP_SC_PEAKING_CUSTOM,
} vip_sc_peaking_sel;


/*
 * Set/Get enable/disable non-linear horizontal scaling.
 */
struct vip_non_linear {
	int	non_linear;		/* 0 => linear, 1 => non-linear */
};

#define VIP_SEL_CSC_COEFFS	_IOW('v', BASE_VIDIOC_PRIVATE + 0,	\
				     vip_csc_coeffs_sel)

#define VIP_SET_CSC_COEFFS	_IOW('v', BASE_VIDIOC_PRIVATE + 1,	\
				     struct vip_colorspace_coeffs)

#define VIP_GET_CSC_COEFFS	_IOW('v', BASE_VIDIOC_PRIVATE + 2,	\
				     struct vip_colorspace_coeffs)

#define VIP_SEL_SC_PEAKING	_IOW('v', BASE_VIDIOC_PRIVATE + 3,	\
				     vip_sc_peaking_sel)

#define VIP_SET_SC_PEAKING	_IOW('v', BASE_VIDIOC_PRIVATE + 4,	\
				     struct vip_sc_peaking)

#define VIP_GET_SC_PEAKING	_IOW('v', BASE_VIDIOC_PRIVATE + 5,	\
				     struct vip_sc_peaking)

#define VIP_SET_NON_LINEAR	_IOW('v', BASE_VIDIOC_PRIVATE + 6,	\
				     struct vip_non_linear)

#define VIP_GET_NON_LINEAR	_IOW('v', BASE_VIDIOC_PRIVATE + 7,	\
				     struct vip_non_linear)

#endif
