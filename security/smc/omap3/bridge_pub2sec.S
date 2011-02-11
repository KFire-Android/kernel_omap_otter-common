/*
 * Copyright (c) 2006-2008 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

.text

#define SMICODEPUB_IRQ_END   0xFE
#define SMICODEPUB_FIQ_END   0xFD
#define SMICODEPUB_RPC_END   0xFC

#ifdef CONFIG_ARM_ERRATA_430973
#define INVALIDATE_BTB       MCR p15, 0, R0, c7, c5, 6
#else
#define INVALIDATE_BTB
#endif

pub2sec_bridge_entry:
   .global pub2sec_bridge_entry

      PUSH     {R4-R12, LR}
      // Copy the Secure Service ID in R12
      MOV      R6, #0xFF
      MOV      R12, R0   // not needed on ES_2_0

      MCR      p15, 0, R0, c7, c5, 4   // Prefetch Buffer flush
      MCR      p15, 0, R0, c7, c10, 4  // Data Synchro Barrier
      INVALIDATE_BTB                // Invalidate entire branch predictor array
      SMC      1
      B        service_end
      NOP
      INVALIDATE_BTB                // Invalidate entire branch predictor array
      BL       SCXLNXSMCommReturnFromIRQ
      MOV      R12, #SMICODEPUB_IRQ_END
      INVALIDATE_BTB                // Invalidate entire branch predictor array
      SMC      1

service_end:
      INVALIDATE_BTB                // Invalidate entire branch predictor array
      POP      {R4-R12, LR}
      BX       LR


rpc_handler:
    .global rpc_handler
      INVALIDATE_BTB                // Invalidate entire branch predictor array
      CPSIE i   //enable IRQs
      BL       SCXLNXSMCommRPCHandler
      MOV      R12, #SMICODEPUB_RPC_END
      INVALIDATE_BTB                // Invalidate entire branch predictor array
      SMC      1

#ifdef COMPILE_AS_LINUX_MODULE

//----------------------------------------------------------------------------
// The following functions have been extracted from the kernel in:
//    > arch\arm\mm\cache-v7.S
//----------------------------------------------------------------------------

/*
 * cache_line_size - get the cache line size from the CSIDR register
 * (available on ARMv7+). It assumes that the CSSR register was configured
 * to access the L1 data cache CSIDR.
 */
   .macro   dcache_line_size, reg, tmp
   mrc   p15, 1, \tmp, c0, c0, 0    @ read CSIDR
   and   \tmp, \tmp, #7       @ cache line size encoding
   mov   \reg, #16         @ size offset
   mov   \reg, \reg, lsl \tmp    @ actual cache line size
   .endm

/*
 * v7_dma_flush_range(start,end)
 * - start   - virtual start address of region
 * - end     - virtual end address of region
 */
v7_dma_flush_range:
   .global v7_dma_flush_range
   dcache_line_size r2, r3
   sub   r3, r2, #1
   bic   r0, r0, r3
1:
   mcr   p15, 0, r0, c7, c14, 1     @ clean & invalidate D / U line
   add   r0, r0, r2
   cmp   r0, r1
   blo   1b
   dsb
   mov   pc, lr


/*
 * v7_dma_inv_range(start,end)
 *
 * Invalidate the data cache within the specified region; we will
 * be performing a DMA operation in this region and we want to
 * purge old data in the cache.
 *
 * - start   - virtual start address of region
 * - end     - virtual end address of region
 */
v7_dma_inv_range:
   .global v7_dma_inv_range
   dcache_line_size r2, r3
   sub   r3, r2, #1
   tst   r0, r3
   bic   r0, r0, r3
   mcrne p15, 0, r0, c7, c14, 1     @ clean & invalidate D / U line

   tst   r1, r3
   bic   r1, r1, r3
   mcrne p15, 0, r1, c7, c14, 1     @ clean & invalidate D / U line
1:
   mcr   p15, 0, r0, c7, c6, 1      @ invalidate D / U line
   add   r0, r0, r2
   cmp   r0, r1
   blo   1b
   dsb
   mov   pc, lr

#endif /* COMPILE_AS_LINUX_MODULE */
