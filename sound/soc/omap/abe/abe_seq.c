/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  BSD LICENSE

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Texas Instruments Incorporated nor the names of
      its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "abe.h"
#include "abe_dbg.h"

#include "abe_mem.h"

/* Maximun subroutines for ABE sequences */
#define OMAP_ABE_MAX_SUB_ROUTINE 10
/* time controlled sequenced */
#define OMAP_ABE_MAX_NB_SEQ 20
/* max number of steps in the sequences */
#define MAXSEQUENCESTEPS 2

/*
 *	SEQ_T
 *
 *	struct {
 *		micros_t time;          Waiting time before executing next line
 *		seq_code_t code         Subroutine index interpreted in the HAL
 *					and translated to FW subroutine codes
 *					in case of ABE tasks
 *		int32 param[2]		Two parameters
 *		} seq_t
 *
 */
struct omap_aess_seq_info {
	u32 delta_time;
	u32 code;
	u32 param[4];
	u8 tag;
};

struct omap_aess_sequence {
	u32 mask;
	struct omap_aess_seq_info seq1;
	struct omap_aess_seq_info seq2;
};

struct omap_aess_subroutine {
	u32 sub_id;
	s32 param[4];
};


const struct omap_aess_sequence seq_null = {
	NOMASK, {CL_M1, 0, {0, 0, 0, 0}, 0}, {CL_M1, 0, {0, 0, 0, 0}, 0}
};
/* table of new subroutines called in the sequence */
abe_subroutine2 abe_all_subsubroutine[OMAP_ABE_MAX_SUB_ROUTINE];
/* number of parameters per calls */
u32 abe_all_subsubroutine_nparam[OMAP_ABE_MAX_SUB_ROUTINE];
/* index of the subroutine */
u32 abe_subroutine_id[OMAP_ABE_MAX_SUB_ROUTINE];
/* paramters of the subroutine (if any) */
u32 abe_all_subroutine_params[OMAP_ABE_MAX_SUB_ROUTINE][4];
/* table of all sequences */
struct omap_aess_sequence abe_all_sequence[OMAP_ABE_MAX_NB_SEQ];
u32 abe_sequence_write_pointer;
/* current number of pending sequences (avoids to look in the table) */
u32 abe_nb_pending_sequences;

/**
 * omap_aess_dummy_subroutine
 *
 */
void omap_aess_dummy_subroutine(void)
{
}

/**
 * omap_aess_add_subroutine
 * @id: ABE port id
 * @f: pointer to the subroutines
 * @nparam: number of parameters
 * @params: pointer to the psrameters
 *
 * add one function pointer more and returns the index to it
 */
void omap_aess_add_subroutine(struct omap_aess *abe, u32 *id, abe_subroutine2 f, u32 nparam, u32 *params)
{
	u32 i, i_found;

	if ((abe->seq.write_pointer >= OMAP_ABE_MAX_SUB_ROUTINE) ||
			((u32) f == 0)) {
		omap_aess_dbg_error(abe, OMAP_ABE_ERR_SEQ,
				   ABE_PARAMETER_OVERFLOW);
	} else {
		/* search if this subroutine address was not already
		 * declared, then return the previous index
		 */
		for (i_found = abe->seq.write_pointer, i = 0;
		     i < abe->seq.write_pointer; i++) {
			if (f == abe_all_subsubroutine[i])
				i_found = i;
		}

		if (i_found == abe->seq.write_pointer) {
			/* Sub routine not listed - Add it */
			*id = abe->seq.write_pointer;
			abe_all_subsubroutine[i_found] = (f);
			for (i = 0; i < nparam; i++)
				abe_all_subroutine_params[i_found][i] = params[i];
			abe_all_subsubroutine_nparam[i_found] = nparam;
			abe->seq.write_pointer++;
		} else {
			/* Sub routine listed - Update parameters */
			for (i = 0; i < nparam; i++)
				abe_all_subroutine_params[i_found][i] = params[i];
			abe_all_subsubroutine_nparam[i_found] = nparam;
			*id = i_found;
		}
	}
}

/**
 * omap_aess_init_subroutine_table - initializes the default table of pointers
 * to subroutines
 *
 * initializes the default table of pointers to subroutines
 *
 */
void omap_aess_init_subroutine_table(struct omap_aess *abe)
{
	u32 id;

	/* reset the table's pointers */
	abe->seq.write_pointer = 0;

	/* the first index is the NULL task */
	omap_aess_add_subroutine(abe, &id, (abe_subroutine2) omap_aess_dummy_subroutine,
			   0, (u32 *) 0);

	omap_aess_add_subroutine(abe, &abe->seq.irq_pingpong_player_id, (abe_subroutine2) omap_aess_dummy_subroutine,
			   0, (u32 *) 0);
}

/**
 * omap_aess_add_sequence
 * @id: returned sequence index after pluging a new sequence
 * (index in the tables)
 * @s: sequence to be inserted
 *
 * Load a time-sequenced operations.
 */
void omap_aess_add_sequence(struct omap_aess *abe, u32 *id, struct omap_aess_sequence *s)
{
	struct omap_aess_seq_info *seq_src, *seq_dst;
	u32 i, no_end_of_sequence_found;
	seq_src = &(s->seq1);
	seq_dst = &((abe_all_sequence[abe_sequence_write_pointer]).seq1);
	if ((abe->seq.write_pointer >= OMAP_ABE_MAX_NB_SEQ) || ((u32) s == 0)) {
		omap_aess_dbg_error(abe, OMAP_ABE_ERR_SEQ,
				   ABE_PARAMETER_OVERFLOW);
	} else {
		*id = abe->seq.write_pointer;
		/* copy the mask */
		(abe_all_sequence[abe->seq.write_pointer]).mask = s->mask;
		for (no_end_of_sequence_found = 1, i = 0; i < MAXSEQUENCESTEPS;
		     i++, seq_src++, seq_dst++) {
			/* sequence copied line by line */
			(*seq_dst) = (*seq_src);
			/* stop when the line start with time=(-1) */
			if ((*(s32 *) seq_src) == (-1)) {
				/* stop when the line start with time=(-1) */
				no_end_of_sequence_found = 0;
				break;
			}
		}
		abe->seq.write_pointer++;
		if (no_end_of_sequence_found)
			omap_aess_dbg_error(abe, OMAP_ABE_ERR_API,
					   ABE_SEQTOOLONG);
	}
}

/**
 * abe_reset_one_sequence
 * @id: sequence ID
 *
 * load default configuration for that sequence
 * kill running activities
 */
void abe_reset_one_sequence(u32 id)
{
}

/**
 * omap_aess_reset_all_sequence
 *
 * load default configuration for all sequences
 * kill any running activities
 */
void omap_aess_reset_all_sequence(struct omap_aess *abe)
{
	u32 i;

	omap_aess_init_subroutine_table(abe);
	/* arrange to have the first sequence index=0 to the NULL operation
	   sequence */
	omap_aess_add_sequence(abe, &i, (struct omap_aess_sequence *) &seq_null);
}

/**
 * omap_aess_call_subroutine
 * @idx: index to the table of all registered Call-backs and subroutines
 *
 * run and log a subroutine
 */
void omap_aess_call_subroutine(struct omap_aess *abe, u32 idx, u32 p1, u32 p2, u32 p3, u32 p4)
{
	abe_subroutine0 f0;
	abe_subroutine1 f1;
	abe_subroutine2 f2;
	abe_subroutine3 f3;
	abe_subroutine4 f4;
	u32 *params;


	if (idx > OMAP_ABE_MAX_SUB_ROUTINE)
		return;

	switch (idx) {
	default:
		switch (abe_all_subsubroutine_nparam[idx]) {
		case 0:
			f0 = (abe_subroutine0) abe_all_subsubroutine[idx];
			(*f0) ();
			break;
		case 1:
			f1 = (abe_subroutine1) abe_all_subsubroutine[idx];
			params = abe_all_subroutine_params[idx];
			if (params != (u32 *) 0)
				p1 = params[0];
			(*f1) (p1);
			break;
		case 2:
			f2 = abe_all_subsubroutine[idx];
			params = abe_all_subroutine_params[idx];
			if (params != (u32 *) 0) {
				p1 = params[0];
				p2 = params[1];
			}
			(*f2) (p1, p2);
			break;
		case 3:
			f3 = (abe_subroutine3) abe_all_subsubroutine[idx];
			params = abe_all_subroutine_params[idx];
			if (params != (u32 *) 0) {
				p1 = params[0];
				p2 = params[1];
				p3 = params[2];
			}
			(*f3) (p1, p2, p3);
			break;
		case 4:
			f4 = (abe_subroutine4) abe_all_subsubroutine[idx];
			params = abe_all_subroutine_params[idx];
			if (params != (u32 *) 0) {
				p1 = params[0];
				p2 = params[1];
				p3 = params[2];
				p4 = params[3];
			}
			(*f4) (p1, p2, p3, p4);
			break;
		default:
			break;
		}
	}
}

/**
 * omap_aess_plug_subroutine
 * @id: returned sequence index after plugging a new subroutine
 * @f: subroutine address to be inserted
 * @n: number of parameters of this subroutine
 * @params: pointer on parameters
 *
 * register a list of subroutines for call-back purpose
 */
int omap_aess_plug_subroutine(struct omap_aess *abe, u32 *id, abe_subroutine2 f, u32 n,
				  u32 *params)
{
	omap_aess_add_subroutine(abe, id, (abe_subroutine2) f, n, (u32 *) params);
	return 0;
}
EXPORT_SYMBOL(omap_aess_plug_subroutine);

