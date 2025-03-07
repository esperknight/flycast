#pragma once
#include "types.h"
#include "sh4_if.h"

#undef sh4op
#define sh4op(str) void  DYNACALL str (u32 op)
typedef void (DYNACALL OpCallFP) (u32 op);

enum OpcodeType
{
	//basic
	Normal       = 0,   // Heh , nothing special :P
	ReadsPC      = 1,   // PC must be set upon calling it
	WritesPC     = 2,   // It will write PC (branch)
	Delayslot    = 4,   // Has a delayslot opcode , valid only when WritesPC is set

	WritesSR     = 8,   // Writes to SR , and UpdateSR needs to be called
	WritesFPSCR  = 16,  // Writes to FPSCR , and UpdateSR needs to be called
	Invalid      = 128, // Invalid

	NO_FP        = 256,
	NO_GP        = 512,
	NO_SP        = 1024,

	UsesFPU      = 2048, // Floating point op
	FWritesFPSCR = UsesFPU | WritesFPSCR,

	// Heh, not basic :P
	ReadWritePC  = ReadsPC|WritesPC,     // Read and writes pc :P
	WritesSRRWPC = WritesSR|ReadsPC|WritesPC,

	// Branches (not delay slot):
	Branch_dir   = ReadWritePC,          // Direct (eg , pc=r[xx]) -- this one is ReadWritePC b/c the delayslot may use pc ;)
	Branch_rel   = ReadWritePC,          // Relative (rg pc+=10);

	// Delay slot
	Branch_dir_d = Delayslot|Branch_dir, // Direct (eg , pc=r[xx])
	Branch_rel_d = Delayslot|Branch_rel, // Relative (rg pc+=10);
};

void ExecuteDelayslot();
void ExecuteDelayslot_RTE();

#define SH4_TIMESLICE 448	// at 112 Bangai-O doesn't start. 224 is ok
							// at 448 Gundam Side Story hangs on Sega copyright screen, 224 ok, 672 ok(!)

extern "C" {

int UpdateSystem();

ATTR_USED int UpdateSystem_INTC();

}
