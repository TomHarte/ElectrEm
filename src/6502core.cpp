/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	6502core.cpp
	============

	Principle parts of 6502 emulation - opcode interpreter, register state
	input and output, reset function

*/

#include "6502.h"
#include <memory.h>

#define FLAG_C				0x01	// carry flag
#define FLAG_Z				0x02	// zero flag
#define FLAG_I				0x04	// interrupt disable flag
#define FLAG_D				0x08	// decimal flag
#define FLAG_B				0x10	// break flag
#define FLAG_A				0x20	// 'always' flag (always set)
#define FLAG_V				0x40	// overflow flag
#define FLAG_N				0x80	// negative flag

#define PLOADFLAGS			(FLAG_A | FLAG_B)

#define EvaluateIRQ()		DoIRQ = IRQLine && !(Flags.Misc&FLAG_I)

#define TrapAddr(v)			TrapFlags[(v) >> 5]&(1 << ((v)&31))

#define LoadVolatiles()		CyclesToRun = VolCyclesToRun; TotalCycleCount = VolTotalCycleCount; SubCycleCount = VolSubCycleCount; FrameCount = VolFrameCount; InstructionsToRun = VolInstructionsToRun;
#define StoreVolatiles()	VolCyclesToRun = CyclesToRun; VolTotalCycleCount = TotalCycleCount; VolSubCycleCount = SubCycleCount; VolFrameCount = FrameCount; VolInstructionsToRun = InstructionsToRun;

#define ReadMem8(addr, val)				val = CMem->Read8Ptrs[(addr) >> 8][(addr)&0xff]
#define ReadMem32(addr, val8, val32)	val8 = CMem->Read8Ptrs[(addr) >> 8][(addr)&0xff]; val32 = CMem->Read32Ptrs[(addr) >> 8][(addr)&0xff]
#define WriteMem8(addr, val)			CMem->Write8Ptrs[(addr) >> 8][(addr)&0xff] = val
#define WriteMem32(addr, val8, val32)	CMem->Write8Ptrs[(addr) >> 8][(addr)&0xff] = val8; CMem->Read32Ptrs[(addr) >> 8][(addr)&0xff] = val32

#define Read8(addr, val)				if(TrapAddr(addr)) QuitEarly = PPPtr->Read(addr, TotalCycleCount, val, TempWord); else ReadMem8(addr, val)
#define Read32(addr, val8, val32)		if(TrapAddr(addr)) QuitEarly = PPPtr->Read(addr, TotalCycleCount, val8, val32); else {ReadMem32(addr, val8, val32);}
#define Write8(addr, val)				if(TrapAddr(addr)) QuitEarly = PPPtr->Write(addr, TotalCycleCount, val, TempWord); else WriteMem8(addr, val)
#define Write32(addr, val8, val32)		if(TrapAddr(addr)) QuitEarly = PPPtr->Write(addr, TotalCycleCount, val8, val32); else {WriteMem32(addr, val8, val32);}

#define ReadMem8BW(addr, val)			val = CMem->Read8Ptrs[addr.b.h][addr.b.l]
#define ReadMem32BW(addr, val8, val32)	val8 = CMem->Read8Ptrs[addr.b.h][addr.b.l]; val32 = CMem->Read32Ptrs[addr.b.h][addr.b.l]
#define WriteMem8BW(addr, val)			CMem->Write8Ptrs[addr.b.h][addr.b.l] = val
#define WriteMem32BW(addr, val8, val32)	CMem->Write8Ptrs[addr.b.h][addr.b.l] = val8; CMem->Read32Ptrs[addr.b.h][addr.b.l] = val32

#define Read8BW(addr, val)				if(TrapAddr(addr.a)) QuitEarly = PPPtr->Read(addr.a, TotalCycleCount, val, TempWord); else ReadMem8BW(addr, val)
#define Read32BW(addr, val8, val32)		if(TrapAddr(addr.a)) QuitEarly = PPPtr->Read(addr.a, TotalCycleCount, val8, val32); else {ReadMem32BW(addr, val8, val32);}
#define Write8BW(addr, val)				if(TrapAddr(addr.a)) QuitEarly = PPPtr->Write(addr.a, TotalCycleCount, val, TempWord); else WriteMem8BW(addr, val)
#define Write32BW(addr, val8, val32)	if(TrapAddr(addr.a)) QuitEarly = PPPtr->Write(addr.a, TotalCycleCount, val8, val32); else {WriteMem32BW(addr, val8, val32);}

#define ReadMem8Z(addrl, val)			val = CMem->Read8Ptrs[0][addrl]
#define ReadMem32Z(addrl, val8, val32)	val8 = CMem->Read8Ptrs[0][addrl]; val32 = CMem->Read32Ptrs[0][addrl]
#define WriteMem8Z(addrl, val)			CMem->Write8Ptrs[0][addrl] = val
#define WriteMem32Z(addrl, val8, val32)	CMem->Write8Ptrs[0][addrl] = val8; CMem->Read32Ptrs[0][addrl] = val32

#define Read8Z(addrl, val)				if(TrapAddr(addrl)) QuitEarly = PPPtr->Read(addrl, TotalCycleCount, val, TempWord); else ReadMem8Z(addrl, val)
#define Read32Z(addrl, val8, val32)		if(TrapAddr(addrl)) QuitEarly = PPPtr->Read(addrl, TotalCycleCount, val8, val32); else {ReadMem32Z(addrl, val8, val32);}
#define Write8Z(addrl, val)				if(TrapAddr(addrl)) QuitEarly = PPPtr->Write(addrl, TotalCycleCount, val, TempWord); else WriteMem8Z(addrl, val)
#define Write32Z(addrl, val8, val32)	if(TrapAddr(addrl)) QuitEarly = PPPtr->Write(addrl, TotalCycleCount, val8, val32); else {WriteMem32Z(addrl, val8, val32);}

#define Modify(addr, op, val8, val32)\
	if(TrapAddr(addr))\
	{\
		QuitEarly = PPPtr->Read(addr, TotalCycleCount, val8, val32); CycleDone(addr);\
		QuitEarly = PPPtr->Write(addr, TotalCycleCount, val8, val32); op; CycleDone(addr);\
		EvaluateIRQ();\
		QuitEarly = PPPtr->Write(addr, TotalCycleCount, val8, val32); CycleDone(addr);\
	}\
	else\
	{\
		val8 = CMem->Read8Ptrs[(addr) >> 8][(addr)&0xff]; val32 = CMem->Read32Ptrs[(addr) >> 8][(addr)&0xff];\
		op; CycleDoneNotEarly(addr); CycleDoneNotEarly(addr);\
		CMem->Write8Ptrs[(addr) >> 8][(addr)&0xff] = val8; CMem->Write32Ptrs[(addr) >> 8][(addr)&0xff] = val32;\
		EvaluateIRQ();\
		CycleDoneNotEarly(addr);\
	}

#define ModifyBW(addr, op, val8, val32)\
	if(TrapAddr(addr.a))\
	{\
		QuitEarly = PPPtr->Read(addr.a, TotalCycleCount, val8, val32); CycleDone(addr.a);\
		QuitEarly = PPPtr->Write(addr.a, TotalCycleCount, val8, val32); op; CycleDone(addr.a);\
		EvaluateIRQ();\
		QuitEarly = PPPtr->Write(addr.a, TotalCycleCount, val8, val32); CycleDone(addr.a);\
	}\
	else\
	{\
		val8 = CMem->Read8Ptrs[addr.b.h][addr.b.l]; val32 = CMem->Read32Ptrs[addr.b.h][addr.b.l];\
		op; CycleDoneNotEarly(addr.a); CycleDoneNotEarly(addr.a);\
		CMem->Write8Ptrs[addr.b.h][addr.b.l] = val8; CMem->Write32Ptrs[addr.b.h][addr.b.l] = val32;\
		EvaluateIRQ();\
		CycleDoneNotEarly(addr.a);\
	}

#define ModifyZ(addr, op, val8, val32)\
	if(TrapAddr(addr))\
	{\
		QuitEarly = PPPtr->Read(addr, TotalCycleCount, val8, val32); CycleDone(0);\
		QuitEarly = PPPtr->Write(addr, TotalCycleCount, val8, val32); op; CycleDone(0);\
		EvaluateIRQ();\
		QuitEarly = PPPtr->Write(addr, TotalCycleCount, val8, val32); CycleDone(0);\
	}\
	else\
	{\
		val8 = CMem->Read8Ptrs[0][addr]; val32 = CMem->Read32Ptrs[0][addr];\
		op; CycleDoneNotEarly(0); CycleDoneNotEarly(0);\
		CMem->Write8Ptrs[0][addr] = val8; CMem->Write32Ptrs[0][addr] = val32;\
		EvaluateIRQ();\
		CycleDoneNotEarly(addr);\
	}

#define Push8(byte)					Write8((0x100 | s), byte); s--
#define Push32(byte, word)			Write32((0x100 | s), byte, word); s--
#define Pull8(byte)					Read8((0x100 | s), byte); s++
#define Pull32(byte, word)			Read32((0x100 | s), byte, word); s++
#define Pull8n(byte)				Read8((0x100 | s), byte)
#define Pull32n(byte, word)			Read32((0x100 | s), byte, word)

#define PushMem8(byte)				WriteMem8(0x100 | s, byte); s--
#define PushMem32(byte, word)		WriteMem32(0x100 | s, byte, word); s--
#define PullMem8(byte)				ReadMem8(0x100 | s, byte); s++
#define PullMem32(byte, word)		ReadMem32(0x100 | s, byte, word); s++
#define PullMem8n(byte)				ReadMem8(0x100 | s, byte)
#define PullMem32n(byte, word)		ReadMem32(0x100 | s, byte, word)

/* carry flag(s) */
#define RD_CARRY()					Flags.Carry
#define RD_CARRY_WD()				Flags.Carry32
#define LD_CARRY(v)					Flags.Carry = v
#define LD_CARRY_WD(v)				Flags.Carry32 = v

/* negative flag */
#define RD_NEG()					(Flags.Neg&0x80)
#define LD_NEG(v)					Flags.Neg = v

/* overflow flag */
#define RD_OVERFLOW()				(Flags.Overflow&0x80)
#define LD_OVERFLOW(v)				Flags.Overflow = v

/* zero flag (set inverted) */
#define RD_ZERO()					Flags.Zero
#define LD_ZERO(v)					Flags.Zero = v

#define LD_NEGZERO(v)				Flags.Neg = Flags.Zero = v

#define RD_STATUS8()				((RD_NEG() ?		FLAG_N : 0) | \
									(RD_ZERO() ?		0 : FLAG_Z) | \
									(RD_OVERFLOW() ?	FLAG_V : 0) | \
									RD_CARRY() | \
									Flags.Misc)

#define RD_STATUS32()				RD_CARRY_WD()

#define SET_STATUS32(v8,v32)		LD_NEG(v8&FLAG_N); \
									LD_ZERO((v8&FLAG_Z)^FLAG_Z); \
									LD_OVERFLOW((v8&FLAG_V)<<1); \
									LD_CARRY(v8&FLAG_C); \
									Flags.Misc =	v8&(FLAG_D | FLAG_I);\
									LD_CARRY_WD(v32&0xf);

#define SetPLoadFlags()				Flags.Misc |= PLOADFLAGS

#define NOP()

#define ADCDecimal()	\
		temp16.b.l = (a8&0x0f) + (NextByte&0x0f) + RD_CARRY();\
\
		temp8 = a8 + NextByte + RD_CARRY();\
		LD_ZERO(temp8);\
\
		if(temp16.b.l > 9) temp16.b.l += 6;\
		temp16.b.h = (a8 >> 4) + (NextByte >> 4) + (temp16.b.l > 15);\
\
		LD_NEG(temp16.b.h<<4);\
		LD_OVERFLOW((a8^temp8) & ~(a8^NextByte));\
\
		if(temp16.b.h > 9) temp16.b.h += 6;\
\
		LD_CARRY((temp16.b.h > 15) ? 1 : 0);\
		a8 = (temp16.b.h << 4) | (temp16.b.l&15)

#define SBCDecimal()	\
		temp16.b.l = (a8&15) - (NextByte&15) - (RD_CARRY()^1);\
		temp16.b.h = (a8 >> 4) - (NextByte >> 4) - ((temp16.b.l&0x10) >> 4);\
\
		if(temp16.b.l&0x10) temp16.b.l -= 6;\
		if(temp16.b.h&0x10) temp16.b.h -= 6;\
\
		temp8 = (temp16.b.h << 4) | (temp16.b.l&15);\
\
		NextByte = ~NextByte;	\
		temp16.a = a8 + NextByte + RD_CARRY();\
\
		LD_NEGZERO(temp16.b.l);\
		LD_CARRY(temp16.b.h ? 1 : 0);\
		LD_OVERFLOW((a8^temp16.b.l) & ~(a8^NextByte));\
\
		a8 = temp8

#define ADCBinary()	\
		temp16.a = a8 + NextByte + RD_CARRY();\
\
		LD_NEGZERO(temp16.b.l);\
		LD_CARRY(temp16.b.h);\
		LD_OVERFLOW((a8^temp16.b.l) & ~(a8^NextByte));\
\
		a8 = temp16.b.l

#define SBCBinary()	\
		temp16.a = a8 + (~NextByte) + RD_CARRY();\
\
		LD_NEGZERO(temp16.b.l);\
		LD_CARRY(temp16.b.h);\
		LD_OVERFLOW((a8^temp16.b.l) & ~(a8^(~NextByte)));\
\
		a8 = temp16.b.l

#define ADC()\
	\
	if(Flags.Misc&FLAG_D)\
	{\
		ADCDecimal();\
	}\
	else\
	{\
		ADCBinary();\
	}

#define SBC()	\
	\
	if(Flags.Misc&FLAG_D)\
	{\
		SBCDecimal();\
	}\
	else\
	{\
		NextByte = ~NextByte;\
		ADCBinary();\
	}

/* ARR is weirdo */
#define ARRBinary()\
	a8 &= NextByte; a32 &= NextWord;\
	a8 = (a8 >> 1) | (RD_CARRY() << 7);\
	LD_NEGZERO(a8);\
	LD_CARRY((a8 >> 6)&1);\
	a32 = (a32 >> 4) | (RD_CARRY_WD() << 7);\
	LD_CARRY_WD((a32 >> 24)&0xf);\
	LD_OVERFLOW((a8 << 2) ^ (a8 << 1))

#define ARRDecimal()\
	a8 &= NextByte;\
	temp16.b.l = a8;\
	a8 = (a8 >> 1) | (RD_CARRY() << 7);\
	LD_ZERO(a8);\
	LD_NEG(RD_CARRY() << 7);\
	LD_OVERFLOW((a8^temp16.b.l) << 1);\
\
	if( ((temp16.b.l&0xf) + (temp16.b.l&0x01)) > 5)\
		a8 = (a8&0xf0) + ((a8+6)&0x0f);\
\
	if( ((temp16.b.l&0xf0) + (temp16.b.l&0x10)) > 0x50)\
	{\
		LD_CARRY(1);\
		a8 += 0x60;\
	}\
	else\
	{\
		LD_CARRY(0);\
	}

#define ARR()\
	if(Flags.Misc&FLAG_D)\
	{\
		ARRDecimal();\
	}\
	else\
	{\
		ARRBinary();\
	}

#define INC(v)	v++; LD_NEGZERO(v)
#define DEC(v)	v--; LD_NEGZERO(v)
#define ANC()\
	a8 &= NextByte;\
	LD_NEGZERO(a8);\
	LD_CARRY(a8 >> 7);\
	a32 &= NextWord;\
	LD_CARRY_WD(a32 >> 28)

#define ALR()\
	AND();\
	LSR(a8, a32)

#define ISC()	INC(NextByte); SBC(); NextByte = ~NextByte
#define RRA()	ROR(NextByte, NextWord); ADC()
#define DCP()	DEC(NextByte); CP(a8, NextByte)

#define AND()\
	a32 &= NextWord;\
	a8 &= NextByte;\
	LD_NEGZERO(a8)

#define ORA()\
	a32 |= NextWord;\
	a8 |= NextByte;\
	LD_NEGZERO(a8)

#define EOR()\
	a32 ^= NextWord;\
	a8 ^= NextByte;\
	LD_NEGZERO(a8)

#define LSR(v, vb)	\
	LD_CARRY(v&1);\
	v >>= 1;\
	LD_NEGZERO(v);\
	LD_CARRY_WD(vb&15);\
	vb >>= 4

#define ASL(v, vb)	\
	LD_CARRY(v>>7);\
	v <<= 1;\
	LD_NEGZERO(v);\
	LD_CARRY_WD(vb>>28);\
	vb <<= 4

#define ROR(v, vb)	\
	temp8 = (v >> 1) | (RD_CARRY() << 7);\
	LD_CARRY(v&1);\
	v = temp8;\
	LD_NEGZERO(v);\
	temp32 = (vb >> 4) | (RD_CARRY_WD() << 28);\
	LD_CARRY_WD(vb&15);\
	vb = temp32

#define ROL(v, vb)	\
	temp16.a = (v << 1) | RD_CARRY();\
	LD_CARRY(temp16.b.h);\
	v = temp16.b.l;\
	LD_NEGZERO(v);\
	temp32 = (vb << 4) | RD_CARRY_WD();\
	LD_CARRY_WD(vb >> 28);\
	vb = temp32

#define SLO()\
	LD_CARRY(NextByte>>7);\
	NextByte <<= 1;\
	a8 |= NextByte;\
	LD_NEGZERO(a8);\
	LD_CARRY_WD(NextWord>>28);\
	NextWord <<= 4;\
	a32 |= NextWord

#define RLA()\
	temp8 = (NextByte << 1) | RD_CARRY();\
	LD_CARRY(NextByte >> 7);\
	a8 &= (NextByte = temp8);\
	LD_NEGZERO(a8);\
	temp32 = (NextWord << 4) | RD_CARRY_WD();\
	LD_CARRY_WD(temp32&15);\
	a32 &= (NextWord = temp32)

#define SRE()\
	LD_CARRY(NextByte&1);\
	NextByte >>= 1;\
	a8 ^= NextByte;\
	LD_NEGZERO(a8);\
	LD_CARRY_WD(a32&15);\
	NextWord >>= 4;\
	a32 ^= NextWord

#define CP(r, v)	\
	temp16.a = r - v;\
	LD_NEGZERO(temp16.b.l);\
	LD_CARRY(temp16.b.h ? 0 : 1)

#define BIT()	\
	LD_ZERO(a8&NextByte);\
	LD_OVERFLOW((NextByte&FLAG_V) << 1);\
	LD_NEG(NextByte)

#define LD(v8, v32)\
	v8 = NextByte;\
	v32 = NextWord;\
	LD_NEGZERO(v8)

#define ST(v8, v32) Write32BW(Addr, v8, v32)
#define STZ(v8, v32) Write32Z(Addr.a, v8, v32)

#define Break()\
		StoreVolatiles();\
		SDL_SemPost(StopSemaphore);\
		SDL_SemWait(GoSemaphore);\
		LoadVolatiles();

#define ILoopRun()\
	{\
		Uint32 Period = CyclesToRun - SubCycleCount;\
\
		SubCycleCount += Period;\
		TotalCycleCount += Period;\
		FrameCount += Period;\
		CycleDownCount -= Period;\
\
		while(Period--)\
			*GatherTarget++ = MemoryPool[*GatherAddresses++];\
\
		StoreVolatiles();\
		SDL_SemPost(StopSemaphore);\
		SDL_SemWait(GoSemaphore);\
		LoadVolatiles();\
	}

#define RunPeriod(n)\
		SubCycleCount += n;\
		TotalCycleCount += n;\
		FrameCount += n;\
		while(n--)\
			*GatherTarget++ = MemoryPool[*GatherAddresses++]

#define CycleDownCount(addr) CMem->ExecCyclePtrs[addr >> 8][FrameCount >> BUS_SHIFT][FrameCount&BUS_MASK]

inline bool C6502::CycleDoneT(int CycleDownCount, MemoryLayout *CMem, bool &QuitEarly)
{
	if(QuitEarly)
	{
		CycleDownCount--; 
		SubCycleCount ++;
		TotalCycleCount ++;
		FrameCount ++;
		*GatherTarget++ = MemoryPool[*GatherAddresses++];
		Break(); if(Quit) return true;
		QuitEarly = false;
	}
	while((CycleDownCount+SubCycleCount) >= CyclesToRun)
	{
		ILoopRun();
		if(Quit) return true;
	}
	RunPeriod(CycleDownCount);
	return false;
}

inline bool C6502::CycleDoneNotEarlyT(int CycleDownCount, MemoryLayout *CMem)
{
	while((CycleDownCount+SubCycleCount) >= CyclesToRun)
	{
		ILoopRun();
		if(Quit) return true;
	}
	RunPeriod(CycleDownCount);
	return false;
}

#define CycleDone(v) if(CycleDoneT(CycleDownCount(v), CMem, QuitEarly)) return 0
#define CycleDoneNotEarly(v) if(CycleDoneNotEarlyT(CycleDownCount(v), CMem)) return 0

/*

	UNOFFICIAL OPCODE DEFINITIONS...

	The following are defined:

	SLO -

		memory is read, shifted left, & or'd with accumulator.
		Sets the N,Z and C flags.

	RLA -

		memory is rolled left, then and'd with the accumulator.
		Sets N, Z and C.

	SRE -

		memory is shifted right, then EOR'd with the accumulator.
		Sets N, Z and C.

	RRA -

		memory is rolled right, then adc'd with the accumulator.
		Sets N, Z, V and C.

	SAX -

		a register and x register are and'd and stored in memory (neither of
		the registers are affected). Sets the N and Z flags

	LAX -

		a register and x register are both loaded simultaneously. Sets the N
		and Z flags

	DCP -

		memory is dec'd, then cmp'd with the accumulator.
		Sets N, Z and C.

	ISC -

		memory is rolled right, then adc'd with the accumulator.
		Sets N, Z, V and C.

	ANC -

		exactly as and, except that carry is set equal to the negative flag

	ALR -

		operand is and'd with accumulator, then accumulator is lsr'd.

	ARR -

		operand is and'd with accumulator, then accumulator is lsr'd
		with weird behaviour in decimal mode

	XAA / ANE -

		puts x into a, then and's with the operand

	AXA[1] -

		stores a&x&(high byte of addr)

	SAY[1] -

 		stores y&(high byte of addr)

	XAS[1] -

 		stores x&(high byte of addr)

	TAS[1] -

		pushes a&x to the stack, then stores a&x&(high byte of addr)

	LAS -

 		stores value&s in a, x and y. Sets N & Z.

*/

/* MSVC 6 build problems [displeasing] workaround */
#ifdef CPU_NOOPT
#pragma optimize( "", off ) 
#endif
int C6502::CPUThread()
{
	Uint32 NextWord, TempWord;
	BrokenWord Addr;
	MemoryLayout *CMem;
	bool QuitEarly = false, DoIRQ, JustWokeUp = false;
	Uint8 Instr =0 , NextByte;

	/* these used to be declared locally for any opcodes that use them,
	but MSVC seems to be much more efficient if they are made local
	to the entire function - and any half decent compiler shouldn't
	care anyway */
	Uint8 temp8;
	BrokenWord temp16, TempAddr;
	Uint32 temp32;

	/* wait to be told to go for the first time */
	SDL_SemWait(GoSemaphore);
	LoadVolatiles();

	while(1)
	{
		while(CPUDead)
		{
			Break();
			JustWokeUp = true;
		}
	
		if(InstructionsToRun)
		{
			if(! --InstructionsToRun)
			{
				Break();
			}
		}

		CMem = CurrentView[pc.b.h >> 5];

		/* fetch instruction */
//		if(pc.a == 0x0f0e)
//		{
//		dump = true;
//		Uint8 Bytes[1];
//		ReadMem8(0xa1, Bytes[0]);
//		printf("0x0f0e: %02x\n",  Bytes[0]);
//		}
#ifdef DUMP_MOTION
		Uint8 Bytes[4];
		ReadMem8(pc.a, Bytes[0]);
		ReadMem8((pc.a+1), Bytes[1]);
		ReadMem8((pc.a+2), Bytes[2]);
		ReadMem8(0xa1, Bytes[3]);
		printf("%04x %02x %02x %02x: a:%02x x:%02x y:%02x s:%02x p:%02x [%02x]\n", pc.a, Bytes[0], Bytes[1], Bytes[2], a8, x8, y8, s, RD_STATUS8(), Bytes[3]);
#endif

		Read8BW(pc, Instr); pc.a++;
		CycleDone(pc.a);

		/* fetch follow-up byte */
		Read32BW(pc, NextByte, NextWord);
		CycleDone(pc.a);

		/* evaluate need for an interrupt in a moment (may be reevaluated by particular instructions) */
		EvaluateIRQ();

		/* perform operation */
		switch(Instr)
		{
			default:
				if(!JustWokeUp)
				{
					PPPtr->Message(PPM_UNKNOWNOP, (void *)Instr);
					CPUDead = true; //halt
				}
				JustWokeUp = false;
				pc.a--;
			break;

			case 0x02: case 0x12: case 0x22: case 0x32:
			case 0x42: case 0x52: case 0x62: case 0x72:
			case 0x92: case 0xb2: case 0xd2: case 0xf2:
				if(!JustWokeUp)
				{
					PPPtr->Message(PPM_CPUDIED, NULL);
					CPUDead = true;	//halt
				}
				JustWokeUp = false;
				pc.a--;
			break;

			/* BRK */
			case 0x00:
				pc.a++;
				if(StackPageClear)
				{
					PushMem8(pc.b.h); CycleDoneNotEarly(0x0100);
					PushMem8(pc.b.l); CycleDoneNotEarly(0x0100);
					PushMem32(RD_STATUS8(), RD_STATUS32()); CycleDoneNotEarly(0x0100);
				}
				else
				{
					Push8(pc.b.h); CycleDone(0x0100);
					Push8(pc.b.l); CycleDone(0x0100);
					Push32(RD_STATUS8(), RD_STATUS32()); CycleDone(0x0100);
				}
				Flags.Misc |= FLAG_I;
				Read8(0xfffe, pc.b.l); CycleDone(0xfffe);
				EvaluateIRQ();
				Read8(0xffff, pc.b.h); CycleDone(0xffff);
			break;

			/* RTI */
			case 0x40:
				s++; CycleDone(0x0100);

				if(StackPageClear)
				{
					PullMem32(NextByte, NextWord); SET_STATUS32(NextByte, NextWord); SetPLoadFlags(); CycleDoneNotEarly(0x0100);
					PullMem8(pc.b.l); CycleDoneNotEarly(0x0100);
					EvaluateIRQ();
					PullMem8n(pc.b.h); CycleDoneNotEarly(0x0100);
				}
				else
				{
					Pull32(NextByte, NextWord); SET_STATUS32(NextByte, NextWord); SetPLoadFlags(); CycleDone(0x0100);
					Pull8(pc.b.l); CycleDone(0x0100);
					EvaluateIRQ();
					Pull8n(pc.b.h); CycleDone(0x0100);
				}
			break;

			/* RTS */
			case 0x60:
				s++; CycleDone(0x0100);
				if(StackPageClear)
				{
					PullMem8(pc.b.l); CycleDoneNotEarly(0x0100);
					PullMem8n(pc.b.h); CycleDoneNotEarly(0x0100);
				}
				else
				{
					Pull8(pc.b.l); CycleDone(0x0100);
					Pull8n(pc.b.h); CycleDone(0x0100);
				}
				EvaluateIRQ();
				CycleDone(pc.a); pc.a++;
			break;

			/* PHA, PHP */
			case 0x48: //PHA
				Push32(a8, a32); CycleDone(0x0100);
			break;

			case 0x08: //PHP
				Push32(RD_STATUS8(), RD_STATUS32()); CycleDone(0x0100);
			break;

			/* PLA, PLP */
			case 0x28: //PLP
				s++; CycleDone(0x0100);
				Pull32n(NextByte, NextWord); SET_STATUS32(NextByte, NextWord); SetPLoadFlags(); CycleDone(0x0100);
			break;

			case 0x68: //PLA
				s++; CycleDone(0x0100);
				Pull32n(a8, a32) 
				LD_NEGZERO(a8); CycleDone(0x0100);
			break;

			/* JSR */
			case 0x20: //JSR
				pc.a++;

				CycleDone(0x0100);	// unknown internal operation
				if(StackPageClear)
				{
					PushMem8(pc.b.h); CycleDoneNotEarly(0x0100);
					PushMem8(pc.b.l); CycleDoneNotEarly(0x0100);
				}
				else
				{
					Push8(pc.b.h); CycleDone(0x0100);
					Push8(pc.b.l); CycleDone(0x0100);
				}

				EvaluateIRQ();
				Read8BW(pc, Addr.b.h); CycleDone(pc.a); pc.b.h = Addr.b.h; pc.b.l = NextByte;
			break;

			/* relative addressing */
#define ConditionalBranch(v)\
		pc.a++;\
		if(v){\
			EvaluateIRQ();\
			Read8BW(pc, temp8); CycleDone(pc.a);\
\
			BrokenWord NewAddr, Offset;\
			Offset.a = (Sint8)NextByte;\
			NewAddr.a = pc.a+Offset.a;\
			if(pc.b.h ^ NewAddr.b.h)\
			{\
				EvaluateIRQ();\
				pc.b.l = NewAddr.b.l;\
				Read8BW(pc, NextByte);\
				CycleDone(pc.a);\
			}\
			pc.a = NewAddr.a;\
		}

			case 0x10: ConditionalBranch(!RD_NEG());		break; //BPL
			case 0x30: ConditionalBranch(RD_NEG());			break; //BMI
			case 0x50: ConditionalBranch(!RD_OVERFLOW());	break; //BVC
			case 0x70: ConditionalBranch(RD_OVERFLOW());	break; //BVS
			case 0x90: ConditionalBranch(!RD_CARRY());		break; //BCC
			case 0xb0: ConditionalBranch(RD_CARRY());		break; //BCS
			case 0xd0: ConditionalBranch(RD_ZERO());		break; //BNE [recall: zero flag is inverted]
			case 0xf0: ConditionalBranch(!RD_ZERO());		break; //BEQ

#undef ConditionalBranch

			/* accumulator or implied addressing */
			case 0x18: LD_CARRY(0);							break; //CLC
			case 0x38: LD_CARRY(1);							break; //SEC
			case 0x58: Flags.Misc &= ~FLAG_I;				break; //CLI
			case 0x78: Flags.Misc |= FLAG_I;				break; //SEI
			case 0xb8: LD_OVERFLOW(0);						break; //CLV
			case 0xd8: Flags.Misc &= ~FLAG_D;				break; //CLD
			case 0xf8: Flags.Misc |= FLAG_D;				break; //SED

			case 0x9a: s = x8;								break; //TXS
			case 0xba: x8 = s; LD_NEGZERO(x8);				break; //TSX
			case 0x98: a8 = y8; LD_NEGZERO(a8); a32 = y32;	break; //TYA
			case 0x8a: a8 = x8; LD_NEGZERO(a8); a32 = x32;	break; //TXA
			case 0xa8: y8 = a8; LD_NEGZERO(y8); y32 = a32;	break; //TAY
			case 0xaa: x8 = a8; LD_NEGZERO(x8); x32 = a32;	break; //TAX

			case 0xe8: INC(x8);								break; //INX
			case 0xc8: INC(y8);								break; //INY

			case 0x88: DEC(y8);								break; //DEY
			case 0xca: DEC(x8);								break; //DEX

			case 0x0a: ASL(a8, a32);						break; //ASL A
			case 0x2a: ROL(a8, a32);						break; //ROL A
			case 0x4a: LSR(a8, a32);						break; //LSR A
			case 0x6a: ROR(a8, a32);						break; //ROR A

			case 0x1a:
			case 0x3a:
			case 0x5a:
			case 0x7a:
			case 0xda:
			case 0xea:
			case 0xfa: NOP();								break;

			/* immediate addressing */

#define ImmediateOp(c)\
	pc.a++;\
	c;

			case 0xa9: ImmediateOp( LD(a8, a32) );	break; //LDA
			case 0xa2: ImmediateOp( LD(x8, x32) );	break; //LDX
			case 0xa0: ImmediateOp( LD(y8, y32) );	break; //LDY

			case 0xe0: ImmediateOp( CP(x8, NextByte) );	break; //CPX
			case 0xc0: ImmediateOp( CP(y8, NextByte) );	break; //CPY
			case 0xc9: ImmediateOp( CP(a8, NextByte) );	break; //CMP

			case 0x09: ImmediateOp(ORA());	break; //ORA
			case 0x29: ImmediateOp(AND());	break; //AND
			case 0x49: ImmediateOp(EOR());	break; //EOR
			case 0x69: ImmediateOp(ADC());	break; //ADC
			case 0xeb:
			case 0xe9: ImmediateOp(SBC());	break; //SBC

			case 0x80:
			case 0x82:
			case 0x89:
			case 0xc2:
			case 0xe2:
				ImmediateOp(NOP());
			break; //NOP

			case 0x0b:
			case 0x2b: ImmediateOp(ANC()); break; //ANC
			case 0x4b: ImmediateOp(ALR()); break; //ALR
			case 0x6b: ImmediateOp(ARR()); break; //ARR
			case 0x8b:
				ImmediateOp(
					a8 = (a8|0xee)&NextByte&x8;
					LD_NEGZERO(a8);
					a32 = (a32|0xfff0fff0)&NextWord&x32;
				);
			break; //ANE
			case 0xab:
				ImmediateOp(
					a8 = x8 = (a8|0xee)&NextByte;
					LD_NEGZERO(a8);
					a32 = x32 = (a32|0xfff0fff0)&NextWord;
				);
			break; //LXA
			case 0xcb:
				ImmediateOp(
						x8 &= a8; x32 &= a32;
						temp16.a = x8 - NextByte;
						x8 = temp16.b.l;
						LD_NEGZERO(temp16.b.l);
						LD_CARRY(temp16.b.h ? 0 : 1);
						x32 -= NextWord;
				);
			break; //SBX

#undef ImmediateOp

		/* absolute addressing */
			case 0x4c:	//JMP
				pc.a++; Addr.b.l = NextByte;
				Read8BW(pc, Addr.b.h); CycleDone(pc.a); pc.a = Addr.a; 
			break;

#define AbsoluteRead(c)\
	pc.a++; Addr.b.l = NextByte;\
	Read8BW(pc, Addr.b.h); CycleDone(pc.a); pc.a++;\
	EvaluateIRQ();\
	Read32BW(Addr, NextByte, NextWord); c; CycleDone(Addr.a);

			case 0xac: AbsoluteRead(LD(y8, y32));				break; //LDY
			case 0xad: AbsoluteRead(LD(a8, a32));				break; //LDA
			case 0xae: AbsoluteRead(LD(x8, x32));				break; //LDX
			case 0xaf: AbsoluteRead(LD(x8 = a8, x32 = a32));	break; //LAX

			case 0x0d: AbsoluteRead(ORA());						break; //ORA
			case 0x2d: AbsoluteRead(AND());						break; //AND
			case 0x4d: AbsoluteRead(EOR());						break; //EOR
			case 0x6d: AbsoluteRead(ADC());						break; //ADC
			case 0xed: AbsoluteRead(SBC());						break; //SBC

			case 0xcc: AbsoluteRead( CP(y8, NextByte) );		break; //CPY
			case 0xcd: AbsoluteRead( CP(a8, NextByte) );		break; //CMP
			case 0xec: AbsoluteRead( CP(x8, NextByte) );		break; //CPX

			case 0x2c: AbsoluteRead( BIT() );					break; //BIT

			case 0x0c: AbsoluteRead( NOP() );					break; //NOP

#undef AbsoluteRead

#define AbsoluteWrite(c)\
	pc.a++; Addr.b.l = NextByte;\
	Read8BW(pc, Addr.b.h); CycleDone(pc.a); pc.a++;\
	EvaluateIRQ();\
	c; CycleDone(Addr.a);

			case 0x8c: AbsoluteWrite( ST(y8, y32) );			break; //STY
			case 0x8d: AbsoluteWrite( ST(a8, a32) );			break; //STA
			case 0x8e: AbsoluteWrite( ST(x8, x32) );			break; //STX
			case 0x8f: AbsoluteWrite( ST(x8&a8, x32&a32) );		break; //SAX

#undef AbsoluteWrite

#define AbsoluteModify(c) \
	pc.a++; Addr.b.l = NextByte;\
	Read8BW(pc, Addr.b.h); CycleDone(pc.a); pc.a++; \
	ModifyBW(Addr, c, NextByte, NextWord)

			case 0x0e: AbsoluteModify(ASL(NextByte, NextWord))		break; //ASL
			case 0x2e: AbsoluteModify(ROL(NextByte, NextWord))		break; //ROL
			case 0x4e: AbsoluteModify(LSR(NextByte, NextWord))		break; //LSR
			case 0x6e: AbsoluteModify(ROR(NextByte, NextWord))		break; //ROR

			case 0xce: AbsoluteModify(DEC(NextByte))				break; //DEC
			case 0xee: AbsoluteModify(INC(NextByte))				break; //INC

			case 0x0f: AbsoluteModify( SLO() )						break; //SLO
			case 0x2f: AbsoluteModify( RLA() )						break; //RLA
			case 0x4f: AbsoluteModify( SRE() )						break; //SRE

			case 0x6f: AbsoluteModify( RRA() )						break; //RRA
			case 0xcf: AbsoluteModify( DCP() )						break; //DCP
			case 0xef: AbsoluteModify( ISC() )						break; //ISC

#undef AbsoluteModify

			/* zero page addressing */
#define ZeroRead(c)\
	pc.a++;\
	EvaluateIRQ();\
	Addr.a = NextByte; Read32Z(Addr.a, NextByte, NextWord); c; CycleDone(0);

			case 0xa4: ZeroRead( LD(y8, y32) );				break; //LDY
			case 0xa5: ZeroRead( LD(a8, a32) );				break; //LDA
			case 0xa6: ZeroRead( LD(x8, x32) );				break; //LDX
			case 0xa7: ZeroRead( LD(x8 = a8, x32 = a32) );	break; //LAX

			case 0x05: ZeroRead( ORA() );					break; //ORA
			case 0x25: ZeroRead( AND() );					break; //AND
			case 0x45: ZeroRead( EOR() );					break; //EOR
			case 0x65: ZeroRead( ADC() );					break; //ADC
			case 0xe5: ZeroRead( SBC() );					break; //SBC

			case 0x24: ZeroRead( BIT() );					break; //BIT
			case 0xc4: ZeroRead( CP(y8, NextByte) );		break; //CPY
			case 0xc5: ZeroRead( CP(a8, NextByte) );		break; //CMP
			case 0xe4: ZeroRead( CP(x8, NextByte) );		break; //CPX

			case 0x04:
			case 0x44:
			case 0x64: ZeroRead( NOP() );					break; //NOP

#undef ZeroRead

#define ZeroWrite(c)\
	pc.a++;\
	EvaluateIRQ();\
	Addr.a = NextByte; c; CycleDone(0);

			case 0x84: ZeroWrite(STZ(y8, y32));			break; //STY
			case 0x85: ZeroWrite(STZ(a8, a32));			break; //STA
			case 0x86: ZeroWrite(STZ(x8, x32));			break; //STX
			case 0x87: ZeroWrite(STZ(a8&x8, a32&x32));	break; //SAX

#undef ZeroWrite

#define ZeroModify(op)\
	pc.a++;\
	Addr.a = NextByte; ModifyZ(Addr.a, op, NextByte, NextWord)

			case 0xc6: ZeroModify( DEC(NextByte) )				break; //DEC
			case 0xe6: ZeroModify( INC(NextByte) )				break; //INC

			case 0x06: ZeroModify( ASL(NextByte, NextWord) )	break; //ASL
			case 0x26: ZeroModify( ROL(NextByte, NextWord) )	break; //ROL
			case 0x46: ZeroModify( LSR(NextByte, NextWord) )	break; //LSR
			case 0x66: ZeroModify( ROR(NextByte, NextWord) )	break; //ROR

			case 0x07: ZeroModify( SLO() )						break; //SLO
			case 0x27: ZeroModify( RLA() )						break; //RLA
			case 0x47: ZeroModify( SRE() )						break; //SRE

			case 0x67: ZeroModify( RRA() )						break; //RRA
			case 0xc7: ZeroModify( DCP() )						break; //DCP
			case 0xe7: ZeroModify( ISC() )						break; //ISC

#undef ZeroModify

			/* zero page indexed addressing */

#define ZeroPageIndexedRead(i, op)\
	pc.a++;\
	if(ZeroPageClear)\
	{\
		ReadMem8Z(NextByte, temp8); NextByte += i; CycleDoneNotEarly(0);\
		EvaluateIRQ();\
		temp8 = NextByte; ReadMem32Z(temp8, NextByte, NextWord); op; CycleDoneNotEarly(0);\
	}\
	else\
	{\
		Read8Z(NextByte, temp8); NextByte += i; CycleDone(0);\
		EvaluateIRQ();\
		temp8 = NextByte; Read32Z(temp8, NextByte, NextWord); op; CycleDone(0);\
	}

			case 0xb4: ZeroPageIndexedRead( x8, LD(y8, y32));				break; //LDY zero,x
			case 0xb5: ZeroPageIndexedRead( x8, LD(a8, a32));				break; //LDA zero,x
			case 0xb6: ZeroPageIndexedRead( y8, LD(x8, x32));				break; //LDX zero,y
			case 0xb7: ZeroPageIndexedRead( y8, LD(x8 = a8, x32 = a32));	break; //LAX zero,y

			case 0x15: ZeroPageIndexedRead( x8, ORA() );					break; //ORA zero,x
			case 0x35: ZeroPageIndexedRead( x8, AND() );					break; //AND zero,x
			case 0x55: ZeroPageIndexedRead( x8, EOR() );					break; //EOR zero,x
			case 0x75: ZeroPageIndexedRead( x8, ADC() );					break; //ADC zero,x
			case 0xf5: ZeroPageIndexedRead( x8, SBC() );					break; //SBC zero,x

			case 0xd5: ZeroPageIndexedRead( x8, CP( a8, NextByte ) );		break; //CMP zero,x

			case 0x14:
			case 0x34:
			case 0x54:
			case 0x74:
			case 0xd4:
			case 0xf4: ZeroPageIndexedRead( x8, NOP() );					break; //NOP

#undef ZeroPageIndexedRead

#define ZeroPageIndexedWrite(i, op)\
	pc.a++; Read8Z(NextByte, temp8); NextByte += i; CycleDone(0);\
	EvaluateIRQ();\
	Addr.a = NextByte; op; CycleDone(0)

			case 0x94: ZeroPageIndexedWrite( x8, STZ(y8, y32));			break; //STY zero,x
			case 0x95: ZeroPageIndexedWrite( x8, STZ(a8, a32));			break; //STA zero,x
			case 0x96: ZeroPageIndexedWrite( y8, STZ(x8, x32));			break; //STX zero,y
			case 0x97: ZeroPageIndexedWrite( y8, STZ(x8&a8, x32&a32));	break; //SAX zero,y

#undef ZeroPageIndexedWrite

#define ZeroPageIndexedModify(op)\
	pc.a++; Read8Z(NextByte, TempAddr.b.l); NextByte += x8; CycleDone(0);\
	TempAddr.b.l = NextByte; ModifyZ(TempAddr.b.l, op, NextByte, NextWord)

			case 0x16: ZeroPageIndexedModify( ASL(NextByte, NextWord) )	break; //ASL
			case 0x36: ZeroPageIndexedModify( ROL(NextByte, NextWord) )	break; //ROL
			case 0x56: ZeroPageIndexedModify( LSR(NextByte, NextWord) )	break; //LSR
			case 0x76: ZeroPageIndexedModify( ROR(NextByte, NextWord) )	break; //ROR

			case 0xd6: ZeroPageIndexedModify( DEC(NextByte) )			break; //DEC
			case 0xf6: ZeroPageIndexedModify( INC(NextByte) )			break; //INC

			case 0x17: ZeroPageIndexedModify( SLO() )					break; //SLO
			case 0x37: ZeroPageIndexedModify( RLA() )					break; //RLA
			case 0x57: ZeroPageIndexedModify( SRE() )					break; //SRE

			case 0x77: ZeroPageIndexedModify( RRA() )					break; //RRA
			case 0xd7: ZeroPageIndexedModify( DCP() )					break; //DCP
			case 0xf7: ZeroPageIndexedModify( ISC() )					break; //ISC

#undef ZeroPageIndexedModify

			/* absolute indexed addressing */
#define AbsoluteIndexedRead(i, op)\
	pc.a++;\
	Addr.b.l = NextByte; Read8BW(pc, Addr.b.h); CycleDone(pc.a); pc.a++;\
	TempAddr.a = Addr.a; TempAddr.b.l += i;\
	Read32BW(TempAddr, NextByte, NextWord); \
	Addr.a += i; if(Addr.b.h^TempAddr.b.h) { CycleDone(TempAddr.a); Read32BW(Addr, NextByte, NextWord); }\
	EvaluateIRQ();\
	op; CycleDone(Addr.a)

			case 0x1d: AbsoluteIndexedRead( x8, ORA() );					break; //ORA abs, x
			case 0x3d: AbsoluteIndexedRead( x8, AND() );					break; //AND abs, x
			case 0x5d: AbsoluteIndexedRead( x8, EOR() );					break; //EOR abs, x
			case 0x7d: AbsoluteIndexedRead( x8, ADC() );					break; //ADC abs, x
			case 0xfd: AbsoluteIndexedRead( x8, SBC() );					break; //SBC abs, x

			case 0x19: AbsoluteIndexedRead( y8, ORA() );					break; //ORA abs, y
			case 0x39: AbsoluteIndexedRead( y8, AND() );					break; //AND abs, y
			case 0x59: AbsoluteIndexedRead( y8, EOR() );					break; //EOR abs, y
			case 0x79: AbsoluteIndexedRead( y8, ADC() );					break; //ADC abs, y
			case 0xf9: AbsoluteIndexedRead( y8, SBC() );					break; //SBC abs, y

			case 0xb9: AbsoluteIndexedRead( y8, LD(a8, a32) );				break; //LDA abs, y

			case 0xbd: AbsoluteIndexedRead( x8, LD(a8, a32) );				break; //LDA abs, x
			case 0xbc: AbsoluteIndexedRead( x8, LD(y8, y32) );				break; //LDY abs, x
			case 0xbe: AbsoluteIndexedRead( y8, LD(x8, x32) );				break; //LDX abs, y
			case 0xbf: AbsoluteIndexedRead( y8, LD(x8 = a8, x32 = a32) );	break; //LAX abs, y

			case 0xdd: AbsoluteIndexedRead( x8, CP(a8, NextByte) );			break; //CMP abs, x
			case 0xd9: AbsoluteIndexedRead( y8, CP(a8, NextByte) );			break; //CMP abs, y

			case 0x1c:
			case 0x3c:
			case 0x5c:
			case 0x7c:
			case 0xdc:
			case 0xfc: AbsoluteIndexedRead( x8, NOP() );					break; //NOP

			case 0xbb:
				AbsoluteIndexedRead( y8, 
					a8 = x8 = s = s&NextByte;
					LD_NEGZERO(a8);
				);
			break; //LAS

#undef AbsoluteIndexedRead

#define AbsoluteIndexedWrite(i, op)\
	pc.a++;\
	Addr.b.l = NextByte; Read8BW(pc, Addr.b.h); CycleDone(pc.a); pc.a++;\
	TempAddr.a = Addr.a; TempAddr.b.l += i;\
	Read32BW(TempAddr, NextByte, NextWord); CycleDone(TempAddr.a);\
	EvaluateIRQ();\
	Addr.a += i; op; CycleDone(Addr.a)

			case 0x99: AbsoluteIndexedWrite( y8, ST(a8, a32));			break; //STA abs, y
			case 0x9d: AbsoluteIndexedWrite( x8, ST(a8, a32));			break; //STA abs, x

			case 0x9b: AbsoluteIndexedWrite( y8,
				s = a8&x8;
				ST(s&(Addr.b.h+1), 0);
			);			break; //SHS abs, y
			case 0x9c: AbsoluteIndexedWrite( x8, ST(y8&(Addr.b.h+1), y32));			break; //SHY abs, x
			case 0x9e: AbsoluteIndexedWrite( y8, ST(x8&(Addr.b.h+1), x32));			break; //SHA abs, y
			case 0x9f: AbsoluteIndexedWrite( y8, ST(a8&x8&(Addr.b.h+1), a32&x32));	break; //SHX abs, y

#undef AbsoluteIndexedWrite

#define AbsoluteIndexedModify(i, op)\
	pc.a++;\
	Addr.b.l = NextByte; Read8BW(pc, Addr.b.h); CycleDone(pc.a); pc.a++;\
	TempAddr.a = Addr.a; TempAddr.b.l += i;\
	Read32BW(TempAddr, NextByte, NextWord); CycleDone(TempAddr.a);\
	Addr.a += i; ModifyBW(Addr, op, NextByte, NextWord)

			case 0x1e: AbsoluteIndexedModify( x8, ASL(NextByte, NextWord) )		break; //ASL abs,x
			case 0x3e: AbsoluteIndexedModify( x8, ROL(NextByte, NextWord) )		break; //ROL abs,x
			case 0x5e: AbsoluteIndexedModify( x8, LSR(NextByte, NextWord) )		break; //LSR abs,x
			case 0x7e: AbsoluteIndexedModify( x8, ROR(NextByte, NextWord) )		break; //ROR abs,x

			case 0xde: AbsoluteIndexedModify( x8, DEC(NextByte) )				break; //DEC abs,x
			case 0xfe: AbsoluteIndexedModify( x8, INC(NextByte) )				break; //INC abs,x

			case 0x1f: AbsoluteIndexedModify( x8, SLO() )						break; //SLO abs,x
			case 0x3f: AbsoluteIndexedModify( x8, RLA() )						break; //RLA abs,x 
			case 0x5f: AbsoluteIndexedModify( x8, SRE() )						break; //SRE abs,x

			case 0x7f: AbsoluteIndexedModify( x8, RRA() )						break; //RRA abs,x
			case 0xdf: AbsoluteIndexedModify( x8, DCP() )						break; //DCP abs,x
			case 0xff: AbsoluteIndexedModify( x8, ISC() )						break; //ISC abs,x

			case 0x1b: AbsoluteIndexedModify( y8, SLO() )						break; //SLO abs,y
			case 0x3b: AbsoluteIndexedModify( y8, RLA() )						break; //RLA abs,y
			case 0x5b: AbsoluteIndexedModify( y8, SRE() )						break; //SRE abs,y

			case 0x7b: AbsoluteIndexedModify( y8, RRA() )						break; //RRA abs,y
			case 0xdb: AbsoluteIndexedModify( y8, DCP() )						break; //DCP abs,y
			case 0xfb: AbsoluteIndexedModify( y8, ISC() )						break; //ISC abs,y

#undef AbsoluteIndexedModify

			/* indexed indirect addressing (zero, x) */

#define IndexedIndirectRead(op)\
	pc.a++;\
	if(ZeroPageClear)\
	{\
		ReadMem8Z(NextByte, temp8); NextByte += x8; CycleDoneNotEarly(0);\
		ReadMem8Z(NextByte, Addr.b.l); NextByte++; CycleDoneNotEarly(0);\
		ReadMem8Z(NextByte, Addr.b.h); CycleDoneNotEarly(0);\
	}\
	else\
	{\
		Read8Z(NextByte, temp8); NextByte += x8; CycleDone(0);\
		Read8Z(NextByte, Addr.b.l); NextByte++; CycleDone(0);\
		Read8Z(NextByte, Addr.b.h); CycleDone(0);\
	}\
	EvaluateIRQ();\
	Read32BW(Addr, NextByte, NextWord); op; CycleDone(Addr.a)

			case 0x01: IndexedIndirectRead( ORA() );					break; //ORA
			case 0x21: IndexedIndirectRead( AND() );					break; //AND
			case 0x41: IndexedIndirectRead( EOR() );					break; //EOR
			case 0x61: IndexedIndirectRead( ADC() );					break; //ADC
			case 0xe1: IndexedIndirectRead( SBC() );					break; //SBC
			case 0xc1: IndexedIndirectRead( CP(a8, NextByte) );			break; //CMP

			case 0xa1: IndexedIndirectRead( LD(a8, a32) );				break; //LDA
			case 0xa3: IndexedIndirectRead( LD(x8 = a8, x32 = a32) );	break; //LAX

#undef IndexedIndirectRead

#define IndexedIndirectWrite(op)\
	pc.a++;\
	if(ZeroPageClear)\
	{\
		ReadMem8Z(NextByte, temp8); NextByte += x8; CycleDoneNotEarly(0);\
		ReadMem8Z(NextByte, Addr.b.l); NextByte++; CycleDoneNotEarly(0);\
		ReadMem8Z(NextByte, Addr.b.h); CycleDoneNotEarly(0);\
	}\
	else\
	{\
		Read8Z(NextByte, temp8); NextByte += x8; CycleDone(0);\
		Read8Z(NextByte, Addr.b.l); NextByte++; CycleDone(0);\
		Read8Z(NextByte, Addr.b.h); CycleDone(0);\
	}\
	EvaluateIRQ();\
	op; CycleDone(Addr.a)

			case 0x81: IndexedIndirectWrite( ST(a8, a32) );			break; //STA
			case 0x83: IndexedIndirectWrite( ST(x8&a8, x32&a32) );	break; //SAX

#undef IndexedIndirectWrite

#define IndexedIndirectModify(op)\
	pc.a++;\
	if(ZeroPageClear)\
	{\
		ReadMem8Z(NextByte, temp8); NextByte += x8; CycleDoneNotEarly(0);\
		ReadMem8Z(NextByte, Addr.b.l); NextByte++; CycleDoneNotEarly(0);\
		ReadMem8Z(NextByte, Addr.b.h); CycleDoneNotEarly(0);\
	}\
	else\
	{\
		Read8Z(NextByte, temp8); NextByte += x8; CycleDone(0);\
		Read8Z(NextByte, Addr.b.l); NextByte++; CycleDone(0);\
		Read8Z(NextByte, Addr.b.h); CycleDone(0);\
	}\
	ModifyBW(Addr, op, NextByte, NextWord)

			case 0x03: IndexedIndirectModify( SLO() )	break; //SLO
			case 0x23: IndexedIndirectModify( RLA() )	break; //RLA
			case 0x43: IndexedIndirectModify( SRE() )	break; //SRE

			case 0x63: IndexedIndirectModify( RRA() )	break; //RRA
			case 0xc3: IndexedIndirectModify( DCP() )	break; //DCP
			case 0xe3: IndexedIndirectModify( ISC() )	break; //ISC

#undef IndexedIndirectModify

			/* indirect indexed addressing (zero), y */
#define IndirectIndexedRead(c)\
	pc.a++;\
	if(ZeroPageClear)\
	{\
		ReadMem8Z(NextByte, Addr.b.l); NextByte++; CycleDoneNotEarly(0);\
		ReadMem8Z(NextByte, Addr.b.h); CycleDoneNotEarly(0);\
	}\
	else\
	{\
		Read8Z(NextByte, Addr.b.l); NextByte++; CycleDone(0);\
		Read8Z(NextByte, Addr.b.h); CycleDone(0);\
	}\
	TempAddr = Addr;\
	TempAddr.b.l += y8;\
	Read32BW(TempAddr, NextByte, NextWord);\
	Addr.a += y8; if(Addr.b.h^TempAddr.b.h) { CycleDone(TempAddr.a); Read32BW(Addr, NextByte, NextWord); }\
	EvaluateIRQ();\
	c; CycleDone(Addr.a)

			case 0x11: IndirectIndexedRead( ORA() );					break; //ORA
			case 0x31: IndirectIndexedRead( AND() );					break; //AND
			case 0x51: IndirectIndexedRead( EOR() );					break; //EOR
			case 0x71: IndirectIndexedRead( ADC() );					break; //ADC
			case 0xf1: IndirectIndexedRead( SBC() );					break; //SBC

			case 0xb1: IndirectIndexedRead( LD(a8, a32));				break; //LDA
			case 0xb3: IndirectIndexedRead( LD(x8 = a8, x32 = a32));	break; //LAX

			case 0xd1: IndirectIndexedRead( CP(a8, NextByte));			break; //CMP

#undef IndirectIndexedRead

#define IndirectIndexedWrite(c)\
	pc.a++;\
	if(ZeroPageClear)\
	{\
		ReadMem8Z(NextByte, Addr.b.l); NextByte++; CycleDoneNotEarly(0);\
		ReadMem8Z(NextByte, Addr.b.h); CycleDoneNotEarly(0);\
	}\
	else\
	{\
		Read8Z(NextByte, Addr.b.l); NextByte++; CycleDone(0);\
		Read8Z(NextByte, Addr.b.h); CycleDone(0);\
	}\
	TempAddr.a = Addr.a;\
	TempAddr.b.l += y8;\
	Read8BW(TempAddr, NextByte); CycleDone(TempAddr.a);\
	EvaluateIRQ();\
	Addr.a += y8;\
	c; CycleDone(Addr.a)

			case 0x91: IndirectIndexedWrite( ST(a8, a32) );						break; //STA
			case 0x93: IndirectIndexedWrite( ST(a8&x8&(Addr.b.h+1), a32&x32));	break; //SHA

#undef IndirectIndexedWrite

#define IndirectIndexedModify(op)\
	pc.a++;\
	if(ZeroPageClear)\
	{\
		ReadMem8Z(NextByte, Addr.b.l); NextByte++; CycleDoneNotEarly(0);\
		ReadMem8Z(NextByte, Addr.b.h); CycleDoneNotEarly(0);\
	}\
	else\
	{\
		Read8Z(NextByte, Addr.b.l); NextByte++; CycleDone(0);\
		Read8Z(NextByte, Addr.b.h); CycleDone(0);\
	}\
	TempAddr.a = Addr.a;\
	TempAddr.b.l += y8;\
	Read8BW(TempAddr, NextByte); CycleDone(TempAddr.a);\
	Addr.a += y8;\
	ModifyBW(Addr, op, NextByte, NextWord)

			case 0x13: IndirectIndexedModify( SLO() )	break; //SLO
			case 0x33: IndirectIndexedModify( RLA() )	break; //RLA
			case 0x53: IndirectIndexedModify( SRE() )	break; //SRE

			case 0x73: IndirectIndexedModify( RRA() )	break; //RRA
			case 0xd3: IndirectIndexedModify( DCP() )	break; //DCP
			case 0xf3: IndirectIndexedModify( ISC() )	break; //ISC

#undef IndexedIndirectModify
			/* absolute indirect addressing */

			case 0x6c: //JMP (addr)
				pc.a++;
				Addr.b.l = NextByte; Read8BW(pc, Addr.b.h); CycleDone(pc.a); pc.a++;
				Read8BW(Addr, pc.b.l); Addr.b.l++; CycleDone(Addr.a);
				EvaluateIRQ();
				Read8BW(Addr, pc.b.h); CycleDone(Addr.a);
			break;
		}

		/* check whether should be IRQ'ing now (actually was checked mid-operation) */
		if(DoIRQ)
		{
			if(StackPageClear)
			{
				PushMem8(pc.b.h); CycleDoneNotEarly(0x0100);
				PushMem8(pc.b.l); CycleDoneNotEarly(0x0100);
				Flags.Misc &= ~FLAG_B;
				PushMem32(RD_STATUS8(), RD_STATUS32());
			}
			else
			{
				Push8(pc.b.h); CycleDone(0x0100);
				Push8(pc.b.l); CycleDone(0x0100);
				Flags.Misc &= ~FLAG_B;
				Push32(RD_STATUS8(), RD_STATUS32());
			}
			Flags.Misc |= FLAG_B | FLAG_I; CycleDone(0x0100);
			Read8(0xfffe, pc.b.l); CycleDone(0xfffe);
			Read8(0xffff, pc.b.h); CycleDone(0xffff);
		}

		/* check for RST */
		if(RSTLine | ForceRST)
		{
			CycleDone(0xfffc);
			CycleDone(0xfffd);
			CycleDone(0xfffc);
			CycleDone(0xfffd);
			Read8(0xfffc, pc.b.l); CycleDone(0xfffc);
			Read8(0xfffd, pc.b.h); CycleDone(0xfffd);

			/* perform reset only on leading edge */
			RSTLine = ForceRST = false;
		}
	}

	return 0;
}
/* MSVC 6 build problems [displeasing] workaround */
#ifdef CPU_NOOPT
#pragma optimize( "", on ) 
#endif

void C6502::GetState(C6502State &st)
{
	st.a32 = a32; st.a8 = a8;
	st.p32 = RD_STATUS32(); st.p8 = RD_STATUS8();
	st.x32 = x32; st.x8 = x8;
	st.y32 = y32; st.y8 = y8;
	st.pc = pc; st.s = s;
}

void C6502::SetState(C6502State &st)
{
	a32 = st.a32; a8 = st.a8;
	SET_STATUS32(st.p8, st.p32);
	SetPLoadFlags();
	x32 = st.x32; x8 = st.x8;
	y32 = st.y32; y8 = st.y8;
	pc = st.pc; s = st.s;
}
