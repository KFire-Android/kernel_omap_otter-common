/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#include "abe_main.h"

/*
 *      SEQUENCES
 *      struct {
 *      micros_t time;	  Waiting time before executing next line
 *      seq_code_t code	 Subroutine index interpreted in the HAL and translated to
 *				FW subroutine codes in case of ABE tasks
 *      int32 param[2]	  Two parameters
 *      char param_tag[2]       Flags used for parameters when launching the sequences
 *      } seq_t
 *
 */

/*
 *  ABE_NULL_SUBROUTINE
 *
 *  Operations : nothing
 */
void abe_null_subroutine_0(void) { }
void abe_null_subroutine_2 (abe_uint32 a, abe_uint32 b) {
	just_to_avoid_the_many_warnings = a;
	just_to_avoid_the_many_warnings = b;
}
void abe_null_subroutine_4 (abe_uint32 a, abe_uint32 b, abe_uint32 c, abe_uint32 d) {
	just_to_avoid_the_many_warnings = a;
	just_to_avoid_the_many_warnings = b;
	just_to_avoid_the_many_warnings = c;
	just_to_avoid_the_many_warnings = d;
}
/*
 *  abe_init_subroutine_table
 *
 *  parameter  :
 *      none
 *
 *  operations :
 *      initializes the default table of pointers to subroutines
 *
 *  return value :
 *
 */
void abe_init_subroutine_table(void)
{
	abe_uint32 id;

	/* reset the table's pointers */
	abe_subroutine_write_pointer = 0;
	/* the first index is the NULL task */
	abe_add_subroutine(&id,(abe_subroutine2) abe_null_subroutine_2, SUB_0_PARAM, (abe_uint32*)0);
	/* write mixer has 3 parameters  @@@ TBD*/
	abe_add_subroutine(&(abe_subroutine_id[SUB_WRITE_MIXER]), (abe_subroutine2) abe_write_mixer, SUB_4_PARAM, (abe_uint32*)0);
	/* ping-pong player IRQ */
	abe_add_subroutine(&abe_irq_pingpong_player_id,(abe_subroutine2) abe_null_subroutine_0, SUB_0_PARAM, (abe_uint32*)0);
	abe_add_subroutine(&abe_irq_aps_adaptation_id,(abe_subroutine2) abe_default_irq_aps_adaptation, SUB_0_PARAM, (abe_uint32*)0);
}

/*
 *  ABE_ADD_SUBROUTINE
 *
 *  Parameter  :
 *      port id
 *      pointer to the subroutines
 *      number of parameters to push on the stack before call
 *
 *  Operations :
 *      add one function pointer more and returns the index to it
 *
 *  Return value :
 *
 */
void abe_add_subroutine (abe_uint32 *id, abe_subroutine2 f, abe_uint32 nparam, abe_uint32* params)
{
	abe_uint32 i, i_found;

	if ((abe_subroutine_write_pointer >= MAXNBSUBROUTINE) || ((abe_uint32)f == 0)) {
		abe_dbg_param |= ERR_SEQ;
		abe_dbg_error_log(ABE_PARAMETER_OVERFLOW);
	} else {
		/* search if this subroutine address was not already
		 * declared, then return the previous index
		 */
		for (i_found = abe_subroutine_write_pointer, i = 0; i < abe_subroutine_write_pointer; i++) {
			if (f == abe_all_subsubroutine[i])
				i_found = i;
		}

		if (i_found == abe_subroutine_write_pointer) {
			*id = abe_subroutine_write_pointer;
			abe_all_subsubroutine[abe_subroutine_write_pointer] = (f);
			abe_all_subroutine_params[abe_subroutine_write_pointer] = params;
			abe_all_subsubroutine_nparam[abe_subroutine_write_pointer] = nparam;
			abe_subroutine_write_pointer++;
		} else {
			abe_all_subroutine_params[i_found] = params;
			*id = i_found;
		}
	}
}

/*
 *  ABE_ADD_SEQUENCE
 *
 *  Parameter  :
 *       Id: returned sequence index after pluging a new sequence (index in the tables)
 *       s : sequence to be inserted
 *
 *  Operations :
 *      Load a time-sequenced operations.
 *
 *  Return value :
 *      None.
 */

void abe_add_sequence(abe_uint32 *id, abe_sequence_t *s)
{
	abe_seq_t *seq_src, *seq_dst;
	abe_uint32 i, no_end_of_sequence_found;

	seq_src = &(s->seq1);
	seq_dst = &((abe_all_sequence[abe_sequence_write_pointer]).seq1);

	if ((abe_sequence_write_pointer >= MAXNBSEQUENCE) || ((abe_uint32)s == 0)) {
		abe_dbg_param |= ERR_SEQ;
		abe_dbg_error_log(ABE_PARAMETER_OVERFLOW);
	} else {
		*id = abe_subroutine_write_pointer;
		(abe_all_sequence[abe_sequence_write_pointer]).mask = s->mask;	/* copy the mask */

		for (no_end_of_sequence_found = 1, i = 0; i < MAXSEQUENCESTEPS; i++, seq_src++, seq_dst++) {
			(*seq_dst) = (*seq_src);	/* sequence copied line by line */

			if ((*(abe_int32 *)seq_src) == -1) {
				/* stop when the line start with time=(-1) */
				no_end_of_sequence_found = 0;
				break;
			}
		}
		abe_subroutine_write_pointer++;

		if (no_end_of_sequence_found)
			abe_dbg_error_log(ABE_SEQTOOLONG);
	}
}

/*
 *  ABE_RESET_ONE_SEQUENCE
 *
 *  Parameter  :
 *      sequence ID
 *
 *  Operations :
 *      load default configuration for that sequence
 *      kill running activities
 *
 *  Return value :
 *
 */
void abe_reset_one_sequence(abe_uint32 id)
{
	just_to_avoid_the_many_warnings = id;
}

/*
 *  ABE_RESET_ALL_SEQUENCE
 *
 *  Parameter  :
 *      none
 *
 *  Operations :
 *      load default configuration for all sequences
 *      kill any running activities
 *
 *  Return value :
 *
 */
void abe_reset_all_sequence(void)
{
	abe_uint32 i;

	abe_init_subroutine_table();

	/* arrange to have the first sequence index=0 to the NULL operation sequence */
	abe_add_sequence(&i, (abe_sequence_t *)&seq_null);

	/* reset the the collision protection mask */
	abe_global_sequence_mask = 0;

	/* reset the pending sequences list */
	for (abe_nb_pending_sequences = i = 0; i < MAXNBSEQUENCE; i++)
		abe_pending_sequences[i] = 0;
}

/*
 *  ABE_CALL_SUBROUTINE
 *
 *  Parameter  :
 *      index to the table of all registered Call-backs and subroutines
 *
 *  Operations :
 *      run and log a subroutine
 *
 *  Return value :
 *      None.
 */
void abe_call_subroutine(abe_uint32 idx, abe_uint32 p1, abe_uint32 p2, abe_uint32 p3, abe_uint32 p4)
{
	abe_subroutine0 f0;
	abe_subroutine1 f1;
	abe_subroutine2 f2;
	abe_subroutine3 f3;
	abe_subroutine4 f4;
	abe_uint32* params;

	if (idx >= MAXNBSUBROUTINE)
		return;

	switch (idx) {
#if 0
	/* call the subroutines defined at compilation time (const .. sequences) */
	case SUB_WRITE_MIXER_DL1 :
		/@@@@ abe_write_mixer_dl1 (p1, p2, p3)
		abe_fprintf ("write_mixer");
		break;
#endif
	/* call the subroutines defined at execution time (dynamic sequences) */
	default :
		switch(abe_all_subsubroutine_nparam[idx]) {
		case SUB_0_PARAM:
			f0 = (abe_subroutine0)abe_all_subsubroutine[idx];
			(*f0)();
			break;
		case SUB_1_PARAM:
			f1 = (abe_subroutine1) abe_all_subsubroutine[idx];
			params = abe_all_subroutine_params[abe_irq_pingpong_player_id];
			if (params != (abe_uint32*)0)
				p1 = params[0];
			(*f1) (p1);
			break;
		case SUB_2_PARAM:
			f2 = abe_all_subsubroutine[idx];
			params = abe_all_subroutine_params[abe_irq_pingpong_player_id];
			if (params != (abe_uint32*)0) {
				p1 = params[0];
				p2 = params[1];
			}
			(*f2) (p1, p2);
			break;
		case SUB_3_PARAM:
			f3 = (abe_subroutine3) abe_all_subsubroutine[idx];
			params = abe_all_subroutine_params[abe_irq_pingpong_player_id];
			if (params != (abe_uint32*)0) {
				p1 = params[0];
				p2 = params[1];
				p3 = params[2];
			}
			(*f3) (p1, p2, p3);
			break;
		case SUB_4_PARAM:
			f4 = (abe_subroutine4) abe_all_subsubroutine[idx];
			params = abe_all_subroutine_params[abe_irq_pingpong_player_id];
			if (params != (abe_uint32*)0) {
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
