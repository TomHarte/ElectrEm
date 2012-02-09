/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	6502misc.cpp
	============

	Miscellaneous parts related to 6502 emulation control - i.e. memory and
	register mappers

	Note on memory format:

		Memory is allocated so as to be 256 bytes of 8bit data followed by
		1024 bytes of 32bit data

*/

#define DevolveAddress(addr, p8, p32)\
	p8 = &((Uint8 *)MemoryPool)[addr + ((addr&~0xff) << 2)];\
	p32 = ((Uint32 *)p8) + 64

#define PAGE_STEP8		1280
#define PAGE_STEP32		320

#include "6502.h"
#include <memory.h>

C6502::C6502()
{
	GoSemaphore = SDL_CreateSemaphore(0);
	StopSemaphore = SDL_CreateSemaphore(0);
	VolInstructionsToRun = 0;

	Quit = false;
	CPUDead = false;
	MainEx = SDL_CreateThread(CPUThreadHelper, this);

//	SetGathering((Uint16 *)AddrTemp, (Uint8 *)AddrTemp, (Uint32 *)AddrTemp);

	AllLayouts = NULL;
	Flags.Carry = Flags.Misc = Flags.Neg = Flags.Overflow = Flags.Zero = 0;
	Flags.Carry32 = 0;

	MemoryPool = NULL;
}

C6502::~C6502()
{
	/* cause CPU thread to exit neatly */
	Quit = true;
	VolSubCycleCount = 0;
	VolCyclesToRun = 1;

	SDL_SemPost(GoSemaphore);
	SDL_WaitThread(MainEx, NULL);
	SDL_DestroySemaphore(GoSemaphore);
	SDL_DestroySemaphore(StopSemaphore);

	delete[] AllLayouts;
	delete[] MemoryPool;
}

void C6502::AttachTo(CProcessPool &pool, Uint32 id)
{
	PPPtr = &pool;
	PPNum = id;

	IOCtl(C6502IOCTL_FINISHOPCODE);
	VolTotalCycleCount = VolFrameCount = 0;

	IRQLine = false;
}

Uint32 C6502::Update(Uint32 t, bool)
{
	if(t)
	{
		VolCyclesToRun = t;
		VolSubCycleCount = 0;

		/* signal main thread to continue for a bit */
		SDL_SemPost(GoSemaphore);
		/* wait for it to finish */
		SDL_SemWait(StopSemaphore);
	}
	else
		VolCyclesToRun = VolSubCycleCount = 0;

	return CYCLENO_ANY;
}

Uint32 C6502::GetCyclesExecuted()
{
	return VolSubCycleCount;
}

void C6502::EstablishMemoryLayouts(int count)
{
	delete[] AllLayouts;
	AllLayouts = new MemoryLayout[count];
}

void C6502::SetMemoryLayout(int id)
{
	CurrentLayout = &AllLayouts[id];
}


void C6502::SetReadPage(int LocalAddr, int GlobalAddr, int Length)
{
	Uint8 *Ptr8;
	Uint32 *Ptr32;
	DevolveAddress(GlobalAddr, Ptr8, Ptr32);

	LocalAddr >>= 8;
	Length >>= 8;
	while(Length--)
	{
		CurrentLayout->Read8Ptrs[LocalAddr] = Ptr8; Ptr8 += PAGE_STEP8;
		CurrentLayout->Read32Ptrs[LocalAddr] = Ptr32; Ptr32 += PAGE_STEP32;

		LocalAddr++;
	}
}

void C6502::SetReadPage32(int LocalAddr, int GlobalAddr, int Length)
{
	Uint8 *Ptr8;
	Uint32 *Ptr32;
	DevolveAddress(GlobalAddr, Ptr8, Ptr32);

	LocalAddr >>= 8;
	Length >>= 8;
	while(Length--)
	{
		CurrentLayout->Read32Ptrs[LocalAddr] = Ptr32; Ptr32 += PAGE_STEP32;
		LocalAddr++;
	}
}

void C6502::SetRepeatedWritePage(int LocalAddr, int GlobalAddr, int Length)
{
	Uint8 *Ptr8;
	Uint32 *Ptr32;
	DevolveAddress(GlobalAddr, Ptr8, Ptr32);

	LocalAddr >>= 8;
	Length >>= 8;
	while(Length--)
	{
		CurrentLayout->Write8Ptrs[LocalAddr] = Ptr8;
		CurrentLayout->Write32Ptrs[LocalAddr] = Ptr32;
		LocalAddr++;
	}
}

void C6502::SetWritePage(int LocalAddr, int GlobalAddr, int Length)
{
	Uint8 *Ptr8;
	Uint32 *Ptr32;
	DevolveAddress(GlobalAddr, Ptr8, Ptr32);

	LocalAddr >>= 8;
	Length >>= 8;
	while(Length--)
	{
		CurrentLayout->Write8Ptrs[LocalAddr] = Ptr8; Ptr8 += PAGE_STEP8;
		CurrentLayout->Write32Ptrs[LocalAddr] = Ptr32; Ptr32 += PAGE_STEP32;
		LocalAddr++;
	}
}

void C6502::SetExecCyclePage(int LocalAddr, Uint32 **Table, int Length)
{
	LocalAddr >>= 8;
	Length >>= 8;
	while(Length--)
	{
		CurrentLayout->ExecCyclePtrs[LocalAddr] = Table;
		LocalAddr++;
	}
}

void C6502::SetGathering(Uint16 *AddressSource, Uint32 *Target)
{
	GatherAddressesStart = GatherAddresses = AddressSource;
	GatherTargetStart = GatherTarget = Target;
	VolFrameCount = VolFrameCount = 0;
}

void C6502::SetMemoryView(int pos, int layout)
{
	CurrentView[pos] = &AllLayouts[layout];
}

bool C6502::IOCtl(Uint32 Control, void *Parameter, Uint32 TimeStamp)
{
	switch(Control)
	{
		/* just do these immediately for now (and maybe forever?) */
		case IOCTL_SETIRQ:
			IRQLine = Parameter ? true : false;
		return true;

		case IOCTL_SETNMI:
			NMILine = Parameter ? true : false;
		return true;

		case IOCTL_SETRST:
			RSTLine = Parameter ? true : false;
		return true;

		case IOCTL_SUPERRESET:
		case IOCTL_RESET:
			ForceRST = true;
			CPUDead = false;
		return true;

		/* trap flags has no meaningful TimeStamp field */
		case IOCTL_NEWTRAPFLAGS:
			TrapFlags = (Uint32 *)Parameter;

			/* 32 addresses per variable => 8 cover 256 */
			ZeroPageClear =
				(TrapFlags[0] | TrapFlags[1] | TrapFlags[2] | TrapFlags[3] |
				TrapFlags[4] | TrapFlags[5] | TrapFlags[6] | TrapFlags[7]) ? false : true;
			StackPageClear =
				(TrapFlags[8] | TrapFlags[9] | TrapFlags[10] | TrapFlags[11] |
				TrapFlags[12] | TrapFlags[13] | TrapFlags[14] | TrapFlags[15]) ? false : true;

		return true;

		case C6502IOCTL_FINISHOPCODE:
		{
			Uint16 Addrs[128];
			Uint32 Targs[128];

			memset(Addrs, 0, sizeof(Uint16)*128);

			Uint16 *GABackup = GatherAddresses; GatherAddresses = Addrs;
			Uint32 *GTBackup = GatherTarget; GatherTarget = Targs;
			Uint32 VolFrameBackup = VolFrameCount; VolFrameCount = 0;

			int OpcodesRemaining = VolInstructionsToRun;
			VolInstructionsToRun = 1;
				VolCyclesToRun = 128; // just to be safe!
				VolSubCycleCount = 0;

				/* signal main thread to continue for a bit */
				SDL_SemPost(GoSemaphore);
				/* wait for it to finish */
				SDL_SemWait(StopSemaphore);

			/* backup stuff temporarily stored away */
			VolInstructionsToRun = OpcodesRemaining ? (OpcodesRemaining-1) : 0;
			GatherAddresses = GABackup;
			GatherTarget = GTBackup;
			VolFrameCount = VolFrameBackup;
		}
		return true;
	}

	return false;
}

void C6502::WriteMem(Uint16 OpAddr, Uint16 WriteAddr, Uint8 Data8, Uint32 Data32)
{
	CurrentView[OpAddr >> 14]->Write8Ptrs[WriteAddr >> 8][WriteAddr&0xff] = Data8;
	CurrentView[OpAddr >> 14]->Write32Ptrs[WriteAddr >> 8][WriteAddr&0xff] = Data32;
}

void C6502::WriteMem(Uint16 OpAddr, Uint16 WriteAddr, Uint8 Data8)
{
	CurrentView[OpAddr >> 14]->Write8Ptrs[WriteAddr >> 8][WriteAddr&0xff] = Data8;
}

void C6502::ReadMem(Uint16 OpAddr, Uint16 ReadAddr, Uint8 &Data8, Uint32 &Data32)
{
	Data8 = CurrentView[OpAddr >> 14]->Read8Ptrs[ReadAddr >> 8][ReadAddr&0xff];
	Data32 = CurrentView[OpAddr >> 14]->Read32Ptrs[ReadAddr >> 8][ReadAddr&0xff];
}

void C6502::ReadMem(Uint16 OpAddr, Uint16 ReadAddr, Uint8 &Data8)
{
	Data8 = CurrentView[OpAddr >> 14]->Read8Ptrs[ReadAddr >> 8][ReadAddr&0xff];
}

void C6502::Pop()
{
	s++;
}

void C6502::Push(Uint8 Data8, Uint32 Data32)
{
	WriteMem(pc.a, 0x100 | s, Data8, Data32); s--;
}

void C6502::SetInstructionLimit(Uint32 Limit)
{
	VolInstructionsToRun = Limit;
}

/*

	Memory allocation, etc

*/

#define Align(n)	if(n&3) n += 4 - (n&3)

bool C6502::SetMemoryTotal(int total)
{
	Align(total);

	delete[] MemoryPool;
	AllocTarget = 0;
	AllocatedBytes = total;
	MemoryPool = new Uint32[total + (total >> 2)];

	return MemoryPool ? true : false;
}

int C6502::GetStorage(int bytes)
{
	int result = AllocTarget;
	AllocTarget += bytes;
	return result;
}

void C6502::WriteMemoryBlock(int Base, int Offset, int Length, Uint8 *Data8, Uint32 *Data32)
{
	Uint8 *Ptr8;
	Uint32 *Ptr32;

	DevolveAddress(Base, Ptr8, Ptr32);

	while(Offset >= 256)
	{
		Ptr8 += PAGE_STEP8;
		Ptr32 += PAGE_STEP32;
		Offset -= 256;
	}

	if(Offset)
	{
		memcpy(Ptr8+Offset, Data8, 256-Offset); Ptr8 += PAGE_STEP8-Offset; Ptr8 += 256 - Offset;
		if(Data32) {memcpy(Ptr32+Offset, Data32, (256-Offset)<<2); Ptr32 += PAGE_STEP32-Offset; Ptr32 += 256 - Offset;}
		Offset = 0;
	}

	while(Length >= 256)
	{
		memcpy(Ptr8, Data8, 256); Data8 += 256; Ptr8 += PAGE_STEP8;
		if(Data32) {memcpy(Ptr32, Data32, 1024); Data32 += 256; Ptr32 += PAGE_STEP32;}
		Length -= 256;
	}
}

void C6502::ReadMemoryBlock(int Base, int Offset, int Length, Uint8 *Data8, Uint32 *Data32)
{
	Uint8 *Ptr8;
	Uint32 *Ptr32;

	DevolveAddress(Base, Ptr8, Ptr32);

	while(Offset >= 256)
	{
		Ptr8 += PAGE_STEP8;
		Ptr32 += PAGE_STEP32;
		Offset -= 256;
	}

	if(Offset)
	{
		memcpy(Data8, Ptr8+Offset, 256-Offset); Ptr8 += PAGE_STEP8-Offset; Ptr8 += 256 - Offset;
		if(Data32) {memcpy(Data32, Ptr32+Offset, (256-Offset)<<2); Ptr32 += PAGE_STEP32-Offset; Ptr32 += 256 - Offset;}
		Offset = 0;
	}

	while(Length >= 256)
	{
		memcpy(Data8, Ptr8, 256); Data8 += 256; Ptr8 += PAGE_STEP8;
		if(Data32) {memcpy(Data32, Ptr32, 1024); Data32 += 256; Ptr32 += PAGE_STEP32;}
		Length -= 256;
	}
}

int C6502::CPUThreadHelper(void *tptr)
{
	return ((C6502 *)tptr)->CPUThread();
}
