#include <linux/kernel.h>
#include "dss.h"

#define YUV422_UYVY     10
#define YUV422_YUV2     11

/* bitpk : used to return partial bit vectors of bigger
 * bit vector, as and when required in algorithm.
 * Ex: bitpk(BaseAddress,28,27) = BaseAddress[28:27]
 */
u32  bitpk(unsigned long int a, u32 left, u32 right)
{
	u32 j, k;
	unsigned long int i;
	i = (a >> right);
	j = ((left == right) ? (u32)1 : (u32)3);
	k =  ((u32)i) & j;
	return k;
}

/* dispc_reg_to_ddma converts the DISPC register values into information
 * used by the DDMA. Ex: format=15 => BytesPerPixel = 3
 * dispcRegConfig :: Dispc register information
 * ChannelNo      :: ChannelNo
 * y_nuv          :: 1->Luma frame parameters, calculation ;
 *		     0->Chroma frame parameters and calculation
 * bh_config       :: Output struct having information useful for the algorithm
 */
void dispc_reg_to_ddma(struct dispc_config *dispc_reg_config, u32 *channel_no,
			 u32 *y_nuv, struct ddma_config *bh_config)
{
	u16 i;
	/* GFX pipe specific conversions */
	if (*channel_no == 0) {
		/* For bitmap formats the pixcel information is stored in bits.
		 * This needs to be divided by 8 to convert into bytes.
		 */
		bh_config->bitmap = (dispc_reg_config->format <= 2) ? 8 : 1;
		/* In case of GFX there is no YUV420 mode:
		 * yuv420: 1->nonYUV420 2-> YUV420
		 */
		bh_config->yuv420 = 1;
		bh_config->pixel_inc    = dispc_reg_config->pixelinc;
		switch (dispc_reg_config->format) {
		/* LUT for format<-->Bytesper pixel */
		case 0:
		case 3:
			i = 1;
			break;
		case 1:
		case 4:
		case 5:
		case 6:
		case 7:
		case 10:
		case 11:
		case 15:
			i = 2;
			break;
		case 9:
			i = 3;
			break;
		default:
			i = 4;
			break;
		}
		bh_config->bpp = i;
		i = 0;
		/* Chroma double_stride value of DISPC registers is invalid
		 * for GFX  where there is np YUV420 format.
		 */
		bh_config->double_stride = 0;
		bh_config->antifckr = dispc_reg_config->antiflicker;
		bh_config->ba = dispc_reg_config->ba ;
		bh_config->size_x = dispc_reg_config->sizex;
	} else {
		/* For all Video channels */
		/* In Video there is no bitmap format, All format pixcel is
		 * stored in multiples of bytes.
		 */
		bh_config->bitmap = 1;
		/* No antiflicker mode for Video channels */
		bh_config->antifckr    = 0;
		/* 1->nonYUV420 2-> YUV420 : Used in breaking up the buffer
		 * allocation:: Top:Luma, Bottom:Chroma
		 */
		bh_config->yuv420 =  (dispc_reg_config->format == 0) ? 2 : 1;

		switch (dispc_reg_config->format) {
		/* LUT for format<-->Bytesper pixel */
		/* bpp:1 for Luma bpp:2 for Chroma */
		case 0:
			i = (*y_nuv ? 1 : 2);
			break;
		case 1:
		case 2:
		case 4:
		case 5:
		case 6:
		case 7:
		case 15:
			i = 2;
			break;
		case 9:
			i = 3;
			break;
		case 10:
		case 11:
			i = (dispc_reg_config->rotation == 1 ||
				dispc_reg_config->rotation == 3) ? 4 : 2;
			break;

		/* Format 10,11 => YUV422. YUV422+No rotation : bpp =bpp/2 */
		default:
			i = 4;
			break;
		}

		/* PixcelIncrement = numberOfPixcelsInterleaving*BytesPerPixcel
		 * + 1. For Chroma pixcelincrement should be doubled to leave
		 * same number of Chroma pixels and Luma.
		 */
		bh_config->pixel_inc = (dispc_reg_config->format == 0) ?
					((*y_nuv == 0) ?
					((dispc_reg_config->pixelinc - 1) * 2) + 1 :
					dispc_reg_config->pixelinc) :
					dispc_reg_config->pixelinc;

		/* for YUV422+No rotation : bpp =bpp/2:: To correct Pixcel
		 * increment accordingly use in stride calculation.
		 */
		bh_config->pixel_inc = (((dispc_reg_config->format == 10) ||
					(dispc_reg_config->format == 11)) &&
					((dispc_reg_config->rotation == 0) ||
					(dispc_reg_config->rotation == 2))) ?
					((dispc_reg_config->pixelinc - 1)/2) + 1 :
					bh_config->pixel_inc;
		bh_config->bpp = i;
		bh_config->double_stride = (dispc_reg_config->format == 0) ?
					 ((*y_nuv == 0) ?
					 dispc_reg_config->doublestride : 0) : 0;

		/* Conditions in which SizeY is halfed = i; */
		i = (((dispc_reg_config->rotation == 1 ||
			dispc_reg_config->rotation == 3)
			&& (dispc_reg_config->format == YUV422_UYVY ||
			dispc_reg_config->format == YUV422_YUV2)) ||
			(((dispc_reg_config->rotation == 1 ||
			dispc_reg_config->rotation == 3) ||
			(bh_config->double_stride == 1)) &&
			((dispc_reg_config->format == 0)
			&& (*y_nuv == 0)))) ? 1 : 0;

		/* Choosing between ba for luma frame and bacbcr for
		 * Chroma frame
		 */
		bh_config->ba = (dispc_reg_config->format == 0) ? ((*y_nuv == 1) ?
				dispc_reg_config->ba : dispc_reg_config->bacbcr) :
				dispc_reg_config->ba;

		/* SizeX halfed for Chroma frame */
		bh_config->size_x = (dispc_reg_config->format == 0) ?
					((*y_nuv == 1) ?
					dispc_reg_config->sizex :
					((dispc_reg_config->sizex + 1) / 2 - 1)) :
					dispc_reg_config->sizex;
	}

	bh_config->twomode = dispc_reg_config->bursttype;
	bh_config->size_y = ((dispc_reg_config->sizey + 1) >> i) - 1;
	bh_config->rowincr = dispc_reg_config->rowinc;
	bh_config->max_burst = 1 << (dispc_reg_config->burstsize + 1);
	/* Decoding the burstSize to be used in BH calculation algorithm */
	bh_config->gballoc = (u32)(dispc_reg_config->gfx_bottom_buffer == *channel_no) +
				(u32)(dispc_reg_config->gfx_top_buffer == *channel_no);
	bh_config->vballoc     = (u32) (dispc_reg_config->vid1_bottom_buffer == *channel_no) +
				(u32)(dispc_reg_config->vid1_top_buffer == *channel_no) +
				(u32) (dispc_reg_config->vid2_bottom_buffer == *channel_no) +
				(u32)(dispc_reg_config->vid2_top_buffer == *channel_no) +
				(u32) (dispc_reg_config->vid3_bottom_buffer == *channel_no) +
				(u32)(dispc_reg_config->vid3_top_buffer == *channel_no) +
				(u32) (dispc_reg_config->wb_bottom_buffer   == *channel_no) +
				(u32)(dispc_reg_config->wb_top_buffer == *channel_no);
 }

/* sa_calc calculates SA and LT values for one set of DISPC reg inputs
 * dispc_reg_config :: Dispc register information
 * channel_no      :: channel_no
 * y_nuv          :: 1->Luma frame parameters, calculation
 *		     0->Chroma frame parameters and calculation
 * sa_info         :: Output struct having information of SA and LT values
 */
void sa_calc(struct dispc_config *dispc_reg_config, u32 *channel_no,
			u32 *y_nuv , struct sa_struct *sa_info)
{
	u32 Sorientation, mode, mode_0, mode_1;
	int blkh_opt;
	int pagemode;
	long int sizeX_nopred, sizeX_pred;
	u32 pict_16word;
	long int pict_16word_ceil;
	long int stride;
	int stride_8k;
	int stride_16k;
	int stride_32k;
	int stride_64k;
	int stride_ok, stride_val ;
	long int linesRem;
	u32 BA_bhbit, bh_max;
	int burstHeight;
	int i;
	int bh2d_cond;
	int C1, c1flag, C2;
	long int Tot_mem;
	struct ddma_config bh_config;

	dispc_reg_to_ddma(dispc_reg_config , channel_no , y_nuv, &bh_config);

	mode         = bitpk(bh_config.ba, (u32)28, (u32)27);
	mode_1       = bitpk(bh_config.ba, (u32)28, (u32)28);
	mode_0       = bitpk(bh_config.ba, (u32)27, (u32)27);
	Sorientation = bitpk(bh_config.ba, (u32)31, (u32)31);

	pagemode = (mode == 3);
	blkh_opt = mode_1 ? 2 : 4 ;

	bh_config.double_stride = (bh_config.double_stride == 1
				&& (bh_config.twomode == 1)) ? 2 : 1 ;

	/* SizeX in frame = number of pixels * BytesPerPixel */
	sizeX_nopred = ((bh_config.size_x + 1) * bh_config.bpp)/
			bh_config.bitmap;
	/* Size including skipped pixels */
	sizeX_pred = ((bh_config.size_x + 1) *
		       (bh_config.pixel_inc-1 + bh_config.bpp)) / bh_config.bitmap;
	stride = ((bh_config.rowincr - 1) + sizeX_pred) * bh_config.double_stride;
	stride_8k = ((stride == 8192) && (mode_1 == 0) && (Sorientation == 1))
			? 1 : 0 ;
	stride_16k = (stride == 16384) && (((mode_0 != mode_1) &&
			(Sorientation == 0)) ? 0 : 1);
	stride_32k = (stride == 32768) && ((mode_1 == 1) ||
			(Sorientation == 0));
	stride_64k = (stride == 65536) && ((mode_0 == mode_1) ? 0 : 1) &&
			(Sorientation == 0);
	stride_ok = (stride_8k || stride_16k || stride_32k || stride_64k);
	stride_val = stride_64k ? 16 : stride_32k ? 15 : stride_16k ? 14 : 13;

	bh_config.size_y = bh_config.size_y + 1;
	linesRem = bh_config.size_y;

	/* Condition than enables 2D fetch of OCP */
	bh2d_cond = (bh_config.twomode && (pagemode == 0)
			&& stride_ok && (linesRem > 0));
	i = 0; C1 = 0  ; c1flag = 0 ; C2 = 0;
	/* BH calculation algorithm depending on stride,NumberofLinesInFrame,
	 * other paarameters of Tiler alignment of base address.
	 */
	for (i = 1; (i <= 5 && (linesRem > 0) && (c1flag == 0)); i++) {
		if (bh2d_cond) {
			/*2D transfer*/
			BA_bhbit = bitpk(bh_config.ba,
					(u32)(stride_val + (mode_1 == 0)),
					(u32)stride_val);
					bh_max = blkh_opt - BA_bhbit;

			if (linesRem > bh_config.max_burst) {
				burstHeight = (bh_config.max_burst > bh_max) ?
						bh_max : bh_config.max_burst;
			} else {
				burstHeight = (linesRem > bh_max) ? bh_max :
						 linesRem;
			}
			burstHeight = ((burstHeight == 3) ||
					(bh_config.antifckr == 1
					&& burstHeight == 4)) ?
					2 : burstHeight;
		} else {
			burstHeight = 1;
		}
		if (((C1 + burstHeight) <= 4) && (c1flag == 0)) {
			/* C1 incremented until its >= 4. ensures howmany
			 * full lines are requested just before SA reaches
			 */
			C1 = C1 + burstHeight;
		} else {
			 if (c1flag == 0) {
				/* After C1 saturated to 4, next burstHeight
				 * decides C2: the number of partially filled
				 * lines when SA condition is reached
				 */
				C2 = burstHeight;
			}
			c1flag = 1 ;
		};
		linesRem = linesRem - burstHeight ;
		bh_config.ba = bh_config.ba + stride * burstHeight ;
	}

	/* Total line buffer memory GFXBuffers+Vid/WB bufers allocated in terms
	 * of 16Byte Word locations
	 */
	Tot_mem = ((640*bh_config.gballoc) + (1024*bh_config.vballoc)) /
				(4*(bh_config.yuv420));
	/* Ceil(rounded to higher integer) of Number of 16Byte Word locations
	 * used by single line of frame.
	 */
	pict_16word_ceil = ((sizeX_nopred%16) > 0) ?
				(sizeX_nopred/16) + 1 : (sizeX_nopred/16);
	/* Exact Number of 16Byte Word locations used by single line of frame */
	pict_16word = ((sizeX_nopred)/16);
	/* Number of sets of 4 lines that can fully fit into the  memory
	 * buffers allocated.
	 */
	i = (Tot_mem/pict_16word_ceil) ;

	sa_info->sa = (long int) (4 * (i - 1) * pict_16word + C1 *
			pict_16word + C2 *
			(Tot_mem - (pict_16word_ceil * i) - 8));

	if (i == 0) {
		/* When line Size is more than line buffer size
		 * (Valid only for 1D)
		 */
		sa_info->min_sa = (Tot_mem - 8);
	} else if (i == 1) {
		/* When MemoryLineBufferSize > LineSize >
		 * (MemoryLineBufferSize/2)
		 */
		sa_info->min_sa = pict_16word +
				C2*(Tot_mem - pict_16word_ceil - 8);
	} else {
		/*All other cases*/
		sa_info->min_sa = (long int)((4 * (i - 2) * pict_16word +
					C1 * pict_16word) + C2 * (Tot_mem -
					(pict_16word_ceil * i) - 8));
	}

	/* C2=0:: no partialy filed lines:: Then minLT = 0 */
	sa_info->min_lt = (C2 == 0) ? 0 : (C2 - 1) * (Tot_mem -
				(pict_16word_ceil * i));
	sa_info->max_lt = ((sa_info->min_sa - 8) > sa_info->min_lt) ?
				 (sa_info->min_sa - 8) : sa_info->min_lt;

	if (bh_config.antifckr == 1) {
		if (C2 == 0)
			sa_info->min_lt = 0;
		else {
			if (C1 == 3)
				sa_info->min_lt = 3 * pict_16word_ceil + C2 *
						 (Tot_mem - (pict_16word_ceil *
						 i));
			else if (C1 == 4)
				sa_info->min_lt = 2 * pict_16word_ceil +
						C2 * (Tot_mem -
						(pict_16word_ceil * i));
		}
	} else {
		 sa_info->min_lt =  (C2 == 0) ? 0 : (C2 - 1) *
					(Tot_mem - (pict_16word_ceil * i));
	}

	sa_info->max_lt = ((sa_info->min_sa - 8) > sa_info->min_lt) ?
				(sa_info->min_sa - 8) : (sa_info->min_lt + 1);

	if ((bh_config.antifckr == 1) &&
		((4 * pict_16word_ceil) > sa_info->max_lt))
			sa_info->min_ht = (4 * pict_16word_ceil) + 8;
	else
		sa_info->min_ht = sa_info->max_lt + 8;

}

/* sa_calc calculates SA and LT values for one set of DISPC reg inputs
 * This takes care of calling the actual sa_calc function once/twice
 *  as per nonYUV420/YUV420 format and gives final value of output
 * dispc_reg_config :: Dispc register information
 * channel_no      :: channel_no
 * y_nuv          :: 1->Luma frame parameters, calculation;
 *		     0->Chroma frame parameters and calculation
 * sa_info         :: Output struct having information of SA and LT values
 */
u32 sa_calc_wrap(struct dispc_config *dispc_reg_config,
				u32 channelno, struct sa_struct *sa_info)
{
	u32 i = 1;
	struct sa_struct sa_info_y;
	struct sa_struct sa_info_uv;

	/*SA values calculated for Luma frame*/
	sa_calc(dispc_reg_config, &channelno, &i , &sa_info_y);
	/*Going into this looop only for YUV420 Format and Channel != GFX*/
	if ((dispc_reg_config->format == 0)  && (channelno > 0)) {
		i = 0;
		/*SA values calculated for Chroma Frame*/
		sa_calc(dispc_reg_config , &channelno, &i, &sa_info_uv);
		sa_info->sa = (sa_info_y.sa > sa_info_uv.sa) ?
				sa_info_uv.sa : sa_info_y.sa;
		/*min_SA = minimum(minSA_Y,minSA_UV)*/
		sa_info->min_sa = (sa_info_y.min_sa > sa_info_uv.min_sa) ?
				sa_info_uv.min_sa : sa_info_y.min_sa;
		/*minLT  = maximum(minLT_Y,minLT_UV)*/
		sa_info->min_lt = (sa_info_y.min_lt > sa_info_uv.min_lt) ?
					sa_info_y.min_lt : sa_info_uv.min_lt;
		/*LT = maximum(minSA-8 , minLT)*/
		sa_info->max_lt = ((sa_info->min_sa - 8) > sa_info->min_lt) ?
				2 * (sa_info->min_sa - 8) :
				2 * (sa_info->min_lt + 1);
		/*HT = maximum(minHT-Y , minHT_UV)*/
		sa_info->min_ht = (sa_info_y.min_ht > sa_info_uv.min_ht) ?
				2 * (sa_info->min_ht) : 2 * (sa_info->min_ht);
	} else {
		sa_info->sa = sa_info_y.sa;
		sa_info->min_sa = sa_info_y.min_sa;
		sa_info->min_lt = sa_info_y.min_lt;
		sa_info->max_lt = sa_info_y.max_lt;
		sa_info->min_ht = sa_info_y.min_ht;
	}

	return sa_info->max_lt;
}


