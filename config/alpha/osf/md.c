/*
 * alpha/osf/md.c
 * OSF/1 Alpha specific functions.
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#include <machine/fpu.h>
#include <sys/sysinfo.h>
#include <machine/hal_sysinfo.h>
#include <sys/proc.h>

#include <assert.h>

#include "md.h"

#ifndef FPCR_INED
#define FPCR_INED 0
#endif
#ifndef FPCR_UNFD
#define FPCR_UNFD 0
#endif
#ifndef FPCR_OVFD
#define FPCR_OVFD 0
#endif
#ifndef FPCR_DZED
#define FPCR_DZED 0
#endif

void
init_md(void)
{
#if 1 /* This doesn't seem to make any difference, but let's keep it.  */
	/* Set the bits in the hw fpcr for cpu's that implement
	   all the bits.  */
	__asm__ __volatile__(
		"excb\n\t"
		"mt_fpcr %0\n\t"
		"excb"
		: : "f"(FPCR_INED | FPCR_UNFD | FPCR_DYN_NORMAL
			| FPCR_OVFD | FPCR_DZED));
#endif

#if 0 /* This breaks DoublePrint and DoubleComp tests.  */
	/* Set the software emulation bits in the kernel for
	   those that don't.  */
	ieee_set_fp_control(IEEE_TRAP_ENABLE_INV);
#endif
}


/* This bit turns off unaligned access fixups in favour of SIGBUS.  It
   is not called by Kaffe, but it is useful to invoke from within 
   the debugger.  */

void alpha_disable_uac(void)
{
  int buf[2];

  buf[0] = SSIN_UACPROC;
  buf[1] = UAC_SIGBUS;
  setsysinfo(SSI_NVPAIRS, buf, 1, 0, 0);
}


#if defined(TRANSLATOR)
/* Native support for exception */

#include <pdsc.h>
#include <excpt.h>

#include "debug.h"

void __alpha_osf_firstFrame (exceptionFrame *frame)
{
	exc_capture_context(&frame->sc);
	DBG(STACKTRACE,
	    dprintf("__alpha_osf_firstFrame(0x%p) pc 0x%p fp 0x%p sp 0x%p\n", frame,
		    (void*)frame->sc.sc_pc, (void*)frame->sc.sc_regs[15],
		    (void*)frame->sc.sc_regs[30]); );
}

exceptionFrame * __alpha_osf_nextFrame (exceptionFrame *frame)
{
	PRUNTIME_FUNCTION pcrd;

	DBG(STACKTRACE,
	    dprintf("__alpha_osf_nextFrame(0x%p) pc 0x%p fp 0x%p sp 0x%p\n", frame,
		    (void*)frame->sc.sc_pc, (void*)frame->sc.sc_regs[15],
		    (void*)frame->sc.sc_regs[30]); );
	pcrd = exc_lookup_function_table (frame->sc.sc_pc);
	DBG(STACKTRACE,
	    dprintf(" pcrd 0x%p\n", pcrd); );
	if (pcrd == NULL) {
		if (DBGEXPR(STACKTRACE, 1, 0)) {
			return NULL;
		}
		else {
			assert (!!"stack trace inspection broken !");
		}

	}
	exc_virtual_unwind (pcrd, &frame->sc);
	DBG(STACKTRACE,
	    dprintf(" -> pc 0x%p fp 0x%p sp 0x%p\n",
		    (void*)frame->sc.sc_pc, (void*)frame->sc.sc_regs[15],
		    (void*)frame->sc.sc_regs[30]); );
	return frame->sc.sc_pc == 0 ? NULL : frame;
}

/* Construct JIT Exception information and register it.  */
void __alpha_osf_register_jit (void *methblock, void *codebase, int codelen)
{
	extern int maxLocal, maxStack, maxTemp, maxArgs, maxPush;
	struct {
		pdsc_crd crd[2];
		struct pdsc_long_stack_rpd rpd;
	} *pdsc = methblock;
	int framesize;			/* frame size in 64 bit words */
	int rsa_offset;			/* rsa offset from $sp in 64 bit words */

	assert (sizeof (*pdsc) == REGISTER_JIT_METHOD_LENGTH);

	/* same as LABEL_Lframe() and LABEL_Lrsa() */
	framesize = maxLocal + maxStack +
		maxTemp + (maxArgs < 6 ? maxArgs : 6) +
		alpha_jit_info.rsa_size +
		(maxPush <= 6 ? 0 : maxPush - 6);
	framesize += framesize & 1;	/* octaword-aligned */
	rsa_offset = (maxPush < 6 ? 0 : maxPush - 6);

	/* create Code Range Descriptor Table */
	pdsc->crd[0].words.begin_address = (pdsc_offset) ((char *) codebase
		- (char *) pdsc);
	pdsc->crd[0].words.rpd_offset = (pdsc_offset) ((char *) &pdsc->rpd
		- (char *) &pdsc->crd[0].words.rpd_offset);
	pdsc->crd[1].words.begin_address = pdsc->crd[0].words.begin_address
		+ codelen;
	pdsc->crd[1].words.rpd_offset = 0;

	/* create Runtime Procedure Descriptor */
	pdsc->rpd.flags = 0;
	pdsc->rpd.entry_ra = 26;
	pdsc->rpd.rsa_offset = rsa_offset;
	pdsc->rpd.sp_set = alpha_jit_info.sp_set;
	pdsc->rpd.entry_length = alpha_jit_info.entry_length;
	pdsc->rpd.frame_size = framesize;
	pdsc->rpd.reserved = 0;
	pdsc->rpd.imask = alpha_jit_info.imask;
	pdsc->rpd.fmask = alpha_jit_info.fmask;

	DBG(STACKTRACE,
	    dprintf("__alpha_osf_register_jit() 0x%p pc [0x%p - 0x%p[\n",
		    methblock, codebase, codebase + codelen);
	    dprintf(" crd[0] begin_address %d rpd_offset\n",
		    pdsc->crd[0].words.begin_address,
		    pdsc->crd[0].words.rpd_offset);
	    dprintf(" crd[1] begin_address %d rpd_offset\n",
		    pdsc->crd[1].words.begin_address,
		    pdsc->crd[1].words.rpd_offset);
	    dprintf(" maxLocal %d maxStack %d maxTemp %d maxArgs %d maxPush %d\n",
		    maxLocal, maxStack, maxTemp, maxArgs, maxPush);
	    dprintf(" framesize %d rsa_offset %d rsa_size %d\n",
		    framesize, rsa_offset, alpha_jit_info.rsa_size);
	    );
	/* Register this runtime procedure descriptor */
	exc_add_pc_range_table (methblock, 2);
	exc_add_gp_range ((exc_address) codebase, codelen, (exc_address) codebase);
}

void __alpha_osf_unregister_jit (void *methblock, void *codebase, int codelen)
{
	DBG(STACKTRACE,
	    dprintf("__alpha_osf_unregister_jit() 0x%p pc [0x%p - 0x%p[\n",
		    methblock, codebase, codebase + codelen) );
	/* Unregister this runtime procedure descriptor */
	exc_remove_pc_range_table (methblock);
	exc_remove_gp_range ((exc_address) codebase);
}
#endif
