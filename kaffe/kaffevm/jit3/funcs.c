/* funcs.c
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#include "config.h"
#include "config-std.h"
#include "gtypes.h"
#include "seq.h"
#include "slots.h"
#include "md.h"
#include "registers.h"
#include "labels.h"
#include "basecode.h"
#include "itypes.h"
#include "md.h"
#include "classMethod.h"
#include "icode.h"

#define	define_insn(n, i) void i (sequence* s)

nativecode* codeblock;
uintp CODEPC;

#define ALIGN(byte)							\
	(CODEPC = (CODEPC % (byte)					\
		   ? CODEPC + (byte) - (CODEPC % (byte))		\
		   : CODEPC))

#undef OUT
#define	OUT	(codeblock[CODEPC++])
#define	BOUT	(*(uint8*)&codeblock[CODEPC++])
#define	WOUT	(*(uint16*)&codeblock[(CODEPC += 2) - 2])
#define	LOUT	(*(uint32*)&codeblock[(CODEPC += 4) - 4])
#define	QOUT	(*(uint64*)&codeblock[(CODEPC += 8) - 8])

#include "jit.def"
