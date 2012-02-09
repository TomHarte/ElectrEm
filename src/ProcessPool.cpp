/*

	ElectrEm (c) 2000-6 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	ProcessPool.cpp
	===============

	Everything ProcessPool related.

	TODO: state saving fix up (time since end of display, tape shift register)

*/

#include <memory.h>

#include "HostMachine/HostMachine.h"
#include "ProcessPool.h"
#include "ComponentBase.h"

#include "6502.h"
#include "Display.h"
#include "ULA.h"
#include "Tape/Tape.h"
#include "Plus3/WD1770.h"
#include "Plus1/Plus1.h"

#include <stdlib.h>
#include "zlib.h"

/* some default ROM slots */
#define ADFS_SLOT1	4
#define ADFS_SLOT2	5
#define ADFS_SLOT	4
#define DFS_SLOT	3
#define PLUS1_SLOT	12

void CProcessPool::TrapTable::Clear()
{
	int c = 256;
	while(c--)
	{
		delete[] TrapAddrDevices[c];
		TrapAddrDevices[c] = NULL;
	}
	memset(TrapAddrFlags, 0, sizeof(Uint32)*2048);
}

CProcessPool::TrapTable::TrapTable()
{
	int c = 256;
	while(c--)
		TrapAddrDevices[c] = NULL;
	memset(TrapAddrFlags, 0, sizeof(Uint32)*2048);
}

CProcessPool::TrapTable::~TrapTable()
{
	int c = 256;
	while(c--)
		delete[] TrapAddrDevices[c];
}

CProcessPool::CProcessPool( ElectronConfiguration &cfg )
{
	AllTrapTables = NULL;
	NumConnectedDevices = 0;
	InTape = false; InTapeTransient = 0;
	CyclesToRun = 0;
	TotalCycles = 0;
	
	/* allocate things we'll definitely need here */
	Disp = new CDisplay(cfg);
	CPU = new C6502;
	ULA = new CULA(cfg);
	Tape = new CTape(cfg);
	Disc = new CWD1770(cfg);
	Plus1 = new CPlus1;

	/* create trap address sets */
	CreateTrapAddressSets(2);

	/* initialise variables related to configuration changes */
	RunningMutex = SDL_CreateMutex();
	MainThreadRunning = false;
	IOCtlMutex = SDL_CreateMutex();
	IOCtlBufferRead = IOCtlBufferWrite = 0;
	ConfigDirty = false;

	/* store configuration for when it might next be useful */
	CurrentConfig = cfg; NextConfig = cfg;
	IOCtl(IOCTL_SETCONFIG, &cfg);
	SetResetConfiguration();
	IOCtl(IOCTL_SUPERRESET);
}

void CProcessPool::SetCycleLimit(Uint32 t)
{
	CyclesToRun = t;
}

CProcessPool::~CProcessPool()
{
	Close((Uint32)-1);

	SDL_DestroyMutex(IOCtlMutex);
	SDL_DestroyMutex(RunningMutex);
	delete CPU;
	delete Disp;
	delete ULA;
	delete Tape;
	delete Disc;
	delete Plus1;
	delete[] AllTrapTables;
}

int CProcessPool::UpdateHelper(void *t)
{
	return ((CProcessPool *)t)->Update();
}

void CProcessPool::GetExclusivity()
{
	if(	MainThreadID != SDL_ThreadID())
		IOCtl(PPOOLIOCTL_MUTEXWAIT);
}

void CProcessPool::ReleaseExclusivity()
{
	if(	MainThreadID != SDL_ThreadID())
		SDL_mutexV(RunningMutex);
}

int CProcessPool::Update()
{
	/* get ID of this thread, so exclusivity works later */
	MainThreadID = SDL_ThreadID();

	/* make sure current configuration is properly propagated */
	IOCtl(IOCTL_SETCONFIG, &NextConfig);

	/* get on with it */
	bool Catchup = false;
	Uint32 Counter = 0, FrameStart = SDL_GetTicks(), FrameCounter = 0, NextCycles, SkippedFrames = 0;

	NextCycles = 0;

	/* this probable needn't go here, but... */
	SendInitialIOCtls();
	IOCtl(IOCTL_UNPAUSE, NULL, TotalCycles);
	MainThreadRunning = true;

	while(!Quit)
	{
		if(IOCtlBufferWrite != IOCtlBufferRead)
		{
			SDL_mutexP(IOCtlMutex);
			while(IOCtlBufferWrite != IOCtlBufferRead)
			{
				switch(IOCtlBuffer[IOCtlBufferRead].Control)
				{
					default:break;

					case PPOOLIOCTL_MUTEXWAIT:
						MainThreadRunning = false;
						SDL_mutexP(RunningMutex);		/* grab running mutex */
						SDL_mutexV(RunningMutex);		/* release running mutex Ñ so anyone waiting on it can have a go */
						MainThreadRunning = true;
					break;

					case PPOOLIOCTL_BREAK:
						/* tell ULA to press break for 1/25 of a second */
						ULA->BeginKeySequence();
							ULA->PressKey(15, 1);
							ULA->KeyWait(80000);
							ULA->ReleaseKey(15, 1);
						ULA->EndKeySequence();
					break;
				}
				IOCtlBufferRead = (IOCtlBufferRead+1)&(IOCTLBUFFER_LENGTH-1);
			}
			SDL_mutexV(IOCtlMutex);
		}

		Uint32 NewCycles = NextCycles, Cyc;

		if(CyclesToRun && NewCycles > CyclesToRun) NewCycles = CyclesToRun;

		NextCycles = CPU->Update(NewCycles, Catchup);
		NewCycles = CPU->GetCyclesExecuted();

		Uint32 c = 1;
		while(c < NumConnectedDevices)
		{
			if(ConnectedDevices[c].Enabled)
			{
				if((Cyc = ConnectedDevices[c].Component->Update(NewCycles, Catchup)) < NextCycles) NextCycles = Cyc;
			}

			c++;
		}

		Counter += NewCycles;
		FrameCounter += NewCycles;
		TotalCycles += NewCycles;

		if(FrameCounter >= 40000) /* end of frame - should be 20ms since last equivalent. Note that ElectrEm pretends all frames are the same length for this real time synchronisation step */
		{
			FrameCounter -= 40000;

			/* Difference = now - start of frame */
#ifndef PROFILE
			Uint32 FrameTime = SDL_GetTicks() - FrameStart;

			if(!InTape)
			{
				if(FrameTime <= 40)
				{
					Catchup = false;
					SkippedFrames = 0;

					if(FrameTime < 20)
						SDL_Delay(20 - FrameTime);
				}
				else
				{
					FrameStart = SDL_GetTicks()-20;
					Catchup = true;
					SkippedFrames++;
					if(SkippedFrames >= 10)
					{
						SkippedFrames = 0;
						Catchup = false;
					}
				}
				FrameStart += 20;
			}
			else
			{
				FrameStart = SDL_GetTicks();
				Catchup = true;
				SkippedFrames++;
				if(SkippedFrames >= 25)
				{
					SkippedFrames = 0;
					Catchup = false;
				}
				if(InTapeTransient)
				{
					InTapeTransient--;
					if(!InTapeTransient)
						InTape = false;
				}
			}
#endif

			/* make sure input continues working */
//			SDL_PumpEvents();

			/* post end of frame message */
			DebugMessage(PPDEBUG_ENDOFFRAME);
		}

		if(CyclesToRun)
		{
			CyclesToRun -= NewCycles;
			if(!CyclesToRun)
			{
				DebugMessage(PPDEBUG_CYCLESDONE);
				Quit = true;
			}
		}
	}

	CyclesToRun = 0;
	MainThreadRunning = false;

	return 0;
}

void CProcessPool::Stop()
{
	Quit = true;
	SDL_WaitThread(UpdateThread, NULL);
	IOCtl(IOCTL_PAUSE, NULL, TotalCycles);
}

void CProcessPool::SetDebugFlags(Uint32 f)
{
	DebugMask = f;
}

void CProcessPool::AddDebugFlags(Uint32 f)
{
	DebugMask |= f;
}

void CProcessPool::RemoveDebugFlags(Uint32 f)
{
	DebugMask &= ~f;
}


int CProcessPool::Go(bool halt)
{
	/* spawn emulation thread, return */
	Quit = false;
	if(halt)
		return Update();
	else
	{
		UpdateThread = SDL_CreateThread(UpdateHelper, this);
	}

	return 0;
}

bool CProcessPool::Write(Uint16 Addr, Uint32 TimeStamp, Uint8 Data8, Uint32 Data32)
{
	if(CurrentTrapTable->TrapAddrDevices[Addr >> 8])	return ConnectedDevices[CurrentTrapTable->TrapAddrDevices[Addr >> 8][Addr&0xff]].Component->Write(Addr, TimeStamp, Data8, Data32);
	return false;
}

bool CProcessPool::Read(Uint16 Addr, Uint32 TimeStamp, Uint8 &Data8, Uint32 &Data32)
{
	if(CurrentTrapTable->TrapAddrDevices[Addr >> 8])	return ConnectedDevices[CurrentTrapTable->TrapAddrDevices[Addr >> 8][Addr&0xff]].Component->Read(Addr, TimeStamp, Data8, Data32);
	return false;
}

void CProcessPool::Close(Uint32 Flags)
{
	if(Flags&PPMEDIA_TAPE)
		Tape->Close();
}

bool CProcessPool::ROMHint(bool present, char *name, int type)
{
	if(!name)
	{
		switch(type)
		{
			case ROMTYPE_ADFS:{
				if(present)
				{
					ULA->InstallROM("%ROMPATH%/adfs-e00_1.rom", ADFS_SLOT1);
					ULA->InstallROM("%ROMPATH%/adfs-e00_2.rom", ADFS_SLOT2);
					ULA->SetROMMode(ADFS_SLOT1, ROMMODE_RAM);
					ULA->SetROMMode(ADFS_SLOT2, ROMMODE_RAM);
//					ULA->InstallROM("%ROMPATH%/adfs.rom", ADFS_SLOT);
				}
				else
				{
//					ULA->SetROMMode(ADFS_SLOT, ROMMODE_OFF);
					ULA->SetROMMode(ADFS_SLOT1, ROMMODE_OFF);
					ULA->SetROMMode(ADFS_SLOT2, ROMMODE_OFF);
				}
			} return true;

			case ROMTYPE_DFS:{
				if(present)
				{
					ULA->InstallROM("%ROMPATH%/DFSE00r3.rom", DFS_SLOT);
					ULA->SetROMMode(DFS_SLOT, ROMMODE_RAM);
				}
				else
					ULA->SetROMMode(DFS_SLOT, ROMMODE_OFF);
			} return true;

			case ROMTYPE_FS:
				if(!present)
				{
					ULA->SetROMMode(DFS_SLOT, ROMMODE_OFF);
					ULA->SetROMMode(ADFS_SLOT, ROMMODE_OFF);
				}
			return true;
		}
	}
	return false;
}

bool CProcessPool::EffectChunk(CUEFChunk *cnk)
{
	bool Used = true;
	cnk->ReadSeek(0, SEEK_SET);

	switch(cnk->GetId())
	{
		default: Used = false; break;

		case 0x0006:{
			Uint8 MType = cnk->GetC();
			if(MType == 1)
				Disp->SetFlags( Disp->GetFlags() | CDF_MULTIPLEXED);
		} break;

		case 0x0007:{
			/* multiplexing palette */

			SDL_Color Palette[256];
			for(int c = 0; c < 256; c++)
			{
				Palette[c].r = cnk->GetC();
				Palette[c].g = cnk->GetC();
				Palette[c].b = cnk->GetC();
			}

			Disp->SetMultiplexedPalette(Palette);
		} break;

		case 0x0008:{ /* ROM hint */
			bool Enabled = (cnk->GetC()&1) ? true : false;

			int Type = 0;
			char *NameString = NULL;
			if(cnk->GetC())
				NameString = cnk->GetString();
			else
				Type = cnk->GetC();

			ROMHint(Enabled, NameString, Type);
			if(NameString) free(NameString);
		} break;

		case 0x0401:{ /* ULA state */
			/* update byte */
			cnk->ReadSeek(1, SEEK_CUR);

			/* interrupt stuff */
			Uint8 IntControl, IntStatus;
			IntControl = cnk->GetC();
			IntStatus = cnk->GetC();
			ULA->Write(0xfe00, TotalCycles, IntControl, 0);
			ULA->AdjustInterrupts(0, 0, IntStatus);

			/* screen start address */
			Uint8 TByte;
			TByte = cnk->GetC();	Disp->Write(0xfe02, TotalCycles, TByte, 0);
			TByte = cnk->GetC();	Disp->Write(0xfe03, TotalCycles, TByte, 0);

			/* cassette shift register */
			cnk->ReadSeek(1, SEEK_CUR);

			/* last page, current ROM */
			Uint8 PageRegister = cnk->GetC();
			Uint8 CurrentROM = cnk->GetC();

			/* do correct paging */
			if(CurrentROM == (PageRegister&0xf))
				ULA->Write(0xfe05, TotalCycles, 0x10, 0);
			ULA->Write(0xfe05, TotalCycles, PageRegister, 0);

			/* fe06 and 7, just pass to ULA */
			int c;
			for(c = 0; c < 2; c++)
			{
				TByte = cnk->GetC();
				ULA->Write(0xfe06 + c, TotalCycles, TByte, 0);
			}

			/* palette */
			for(c = 0; c < 8; c++)
			{
				TByte = cnk->GetC();
				Disp->Write(0xfe08 + c, TotalCycles, TByte, 0);
			}
		} break;

		case 0x0400:{ /* 6502 standard state */

			/* update byte */
			cnk->ReadSeek(1, SEEK_CUR);
			CPU->IOCtl(C6502IOCTL_FINISHOPCODE);

			C6502State st;
			CPU->GetState(st);

			st.a8 = cnk->GetC();
			st.p8 = cnk->GetC();
			st.x8 = cnk->GetC();
			st.y8 = cnk->GetC();
			st.s = cnk->GetC();
			st.pc.a = cnk->Get16();

			CPU->SetState(st);
		}break;

		case 0x0412:{ /* 6502 extended state */

			/* update byte */
			cnk->ReadSeek(1, SEEK_CUR);

			C6502State st;
			CPU->GetState(st);

			st.a32 = cnk->Get32();
			st.p32 = cnk->Get32();
			st.x32 = cnk->Get32();
			st.y32 = cnk->Get32();

			CPU->SetState(st);
		}break;

		case 0x0410:{ /* standard memory state */

			/* update byte */
			cnk->ReadSeek(1, SEEK_CUR);

			int Ptr;
			bool Load = false;

			switch(cnk->GetC())
			{
				case 0: Ptr = ULA->GetMappedAddr(MEM_RAM); Load = true; break;
				case 1: Ptr = ULA->GetMappedAddr(MEM_SHADOW); Load = true; break;
			}

			if(Load && cnk->GetLength() > 2 && cnk->GetLength() <= 32770)
			{
				Uint8 *DataPtr = new Uint8[cnk->GetLength() - 2];
				cnk->Read(DataPtr, cnk->GetLength() - 2);
				CPU->WriteMemoryBlock(Ptr, 0, cnk->GetLength() - 2, DataPtr);
				delete[] DataPtr;
			}
		}break;

		case 0x0411:{ /* multiplexed memory state */

			// update byte
			cnk->ReadSeek(1, SEEK_CUR);

/*			Uint32 *Ptr;

			switch(cnk->GetC())
			{
				default: Ptr = NULL; break;
				case 0: Ptr = MemoryPool32; break;
			}

			if(Ptr)
			{
				cnk->Read(Ptr, cnk->GetLength() - 2);

				// reorder bytes here if necessary

//				if(OldMultiplexing)
				{
					int c = cnk->GetLength() - 2;
					while(c--)
						Ptr[c] |= Ptr[c] << 16;
				}
			}*/
		}break;
		
		case 0x0420:	/* MRB state */
		{
			ElectronConfiguration NewConfig = NextConfig;
			cnk->ReadSeek(1, SEEK_CUR);
			switch(cnk->GetC())
			{
				default: NewConfig.MRBMode = MRB_OFF; break;
				case 1: NewConfig.MRBMode = MRB_TURBO; break;
				case 2: NewConfig.MRBMode = MRB_SHADOW; break;
			}
			
			/* TODO: and ... ? */
		}break;
	}

	return Used;
}

#define UEF_VERSION 0x000a

#define RF_TAPE	0x01
#define RF_ADFS	0x02
#define RF_DFS	0x04

bool CProcessPool::Open(char *fname)
{
	GetExclusivity();	//can't do an open while the emulation is running

	bool Used = false;
	char *UEFExts[] = { "uef\a", NULL };
	bool UEF = GetHost()->ExtensionIncluded(fname, UEFExts);
	int NewROMFlags = 0;

	/* consider: is this a tape file of any description? */
	if(Tape->Open(fname))
	{
		NewROMFlags |= RF_TAPE;
		/* we have a tape file, so do not attempt to parse snapshot chunks now */
		Used = true;

		const char *cmd = Tape->QueryTapeCommand();
		if(cmd && CurrentConfig.Autoload)
		{
			/* exclusivity may be needed to perform a superreset, because it may involve memory map changes/etc, so release it for the time being */
			ReleaseExclusivity();
			IOCtl(IOCTL_SUPERRESET);
			GetExclusivity();	//can't do an open while the emulation is running

			ULA->BeginKeySequence();
				ULA->PressKey(0, 0);
				ULA->KeyWait(ELECTRON_RESET_TIME);
				ULA->TypeASCII(cmd);
			ULA->EndKeySequence();
		}

		/* do control chunks */
		if(UEF)
		{
			CUEFChunkSelector *ControlChunks =  GetHost()  -> GetUEFSelector(fname, UEF_VERSION);

			if(ControlChunks)
			{
				ControlChunks->ResetOverRan();

				while(!ControlChunks->OverRan())
				{
					if(ControlChunks->FindIdMajor(0x00))
						EffectChunk(ControlChunks->CurrentChunk());
				}

				GetHost()  -> ReleaseUEFSelector(ControlChunks);
			}
		}
	}
	else
		if(UEF)
		{
			/* if not a tape, parse all chunks, snapshot or otherwise, here and now */
			CUEFChunkSelector *Snapshot =  GetHost() -> GetUEFSelector(fname, UEF_VERSION);
			if(Snapshot)
			{
				Snapshot->ResetOverRan();

				while(!Snapshot->OverRan())
				{
					Used |= EffectChunk(Snapshot->CurrentChunk());
					Snapshot->Seek(1, SEEK_CUR);
				}

				GetHost() -> ReleaseUEFSelector(Snapshot);
			}
		}

	/* give disc a go */
	int DiscMode;
	if(DiscMode = Disc->Open(fname))
	{
		NewROMFlags |= (DiscMode == WDOPEN_DDEN) ? RF_ADFS : RF_DFS;
		if(!Used && CurrentConfig.Autoload)
		{
			ReleaseExclusivity();
			IOCtl(IOCTL_SUPERRESET);
			GetExclusivity();

			ULA->BeginKeySequence();
				ULA->PressKey(0, 0);
				ULA->KeyWait(39936*50);

				// press shift
				ULA->PressKey(13, 8);

				// press 'A' or 'D'
				if(DiscMode == WDOPEN_DDEN)
					ULA->PressKey(12, 4);
				else
					ULA->PressKey(10, 4);

				// press break
				ULA->PressKey(15, 1);
				ULA->KeyWait(39936);

				// release break
				ULA->ReleaseKey(15, 1);
				ULA->KeyWait(39936);

				// release shift
				ULA->ReleaseKey(13, 8);
				ULA->KeyWait(39936*50);

			ULA->EndKeySequence();
		}
		Used = true;
	}

	// auto configure ROMs & attached devices if allowed
	if(Used && CurrentConfig.Autoconfigure)
	{
		ElectronConfiguration Config2 = CurrentConfig;
		switch(NewROMFlags)
		{
			default:
				Config2.Plus3.Enabled = true;
				ROMHint((NewROMFlags&RF_ADFS) ? true : false, NULL, ROMTYPE_ADFS);
				ROMHint((NewROMFlags&RF_DFS) ? true : false, NULL, ROMTYPE_DFS);
			break;

			case RF_TAPE:
				Config2.Plus3.Enabled = false;
				ROMHint(false, NULL, ROMTYPE_FS);
			break;
		}
		ReleaseExclusivity();
		IOCtl(IOCTL_SETCONFIG, &Config2);
		GetExclusivity();
	}

	ReleaseExclusivity();	//let the emulator go again if it wants
	return Used;
}

#undef RF_TAPE
#undef RF_DISC

bool CProcessPool::SaveState(char *fname)
{
	/* check for .uef ending, add it if it is missing */
	char *locname;
	char *UEFExts[] = { "uef\a", NULL };
	if(!GetHost()->ExtensionIncluded(fname, UEFExts))
	{
		locname = (char *)malloc(strlen(fname) + 5);
		sprintf(locname, "%s.uef", fname);
	}
	else
		locname = fname;

	/* get a UEF selector, for writing data to */
	CUEFChunkSelector *SaveFile = GetHost() -> GetUEFSelector(locname, 0x0006);//UEF_VERSION);
	if(!SaveFile) return false;

	GetExclusivity();

	/* advance CPU to end of opcode, so that state is meaningful */
	CPU->IOCtl(C6502IOCTL_FINISHOPCODE);

	/* delete any state chunks */
	SaveFile->ResetOverRan();
	while(SaveFile->FindIdMajor(0x04))
		SaveFile->RemoveCurrentChunk();

	/* do save */
		/* CPU Ñ always needed */
			SaveFile->EstablishChunk();
			SaveFile->CurrentChunk()->SetId(0x0400);

			/* get CPU state */
			C6502State s;
			CPU->GetState(s);

			/* write CPU state */
			SaveFile->CurrentChunk()->PutC(0);			// 'update byte' - no updates!
			SaveFile->CurrentChunk()->PutC(s.a8);		// a register
			SaveFile->CurrentChunk()->PutC(s.p8);		// p (status) register
			SaveFile->CurrentChunk()->PutC(s.x8);		// x register
			SaveFile->CurrentChunk()->PutC(s.y8);		// y register
			SaveFile->CurrentChunk()->PutC(s.s);		// s (stack pointer) register
			SaveFile->CurrentChunk()->Put16(s.pc.a);	// program counter

		/* ULA - always needed */
			SaveFile->EstablishChunk();
			SaveFile->CurrentChunk()->SetId(0x0401);
			
			/* write ULA state */
			SaveFile->CurrentChunk()->PutC(0);			// 'update byte' - no updates!
			SaveFile->CurrentChunk()->PutC( ULA->QueryRegister(ULAREG_INTCONTROL) );		// interrupt control
			SaveFile->CurrentChunk()->PutC( ULA->QueryRegister(ULAREG_INTSTATUS) );		// interrupt status
			SaveFile->CurrentChunk()->PutC( Disp->Read(0xfe02) );		// fe02 (scr addr low)
			SaveFile->CurrentChunk()->PutC( Disp->Read(0xfe03) );		// fe03 (scr addr high)
			SaveFile->CurrentChunk()->PutC(0);			// cassette shift register
			SaveFile->CurrentChunk()->PutC( ULA->QueryRegister(ULAREG_PAGEREGISTER) );		// last value written to fe05
			SaveFile->CurrentChunk()->PutC( ULA->QueryRegister(ULAREG_LASTPAGED) );		// currently paged ROM
			
			/* sheila bytes fe06 -> fe0f */
			SaveFile->CurrentChunk()->PutC( ULA->QueryRegAddr(0xfe06) );		// fe06 (clock divider)
			SaveFile->CurrentChunk()->PutC( ULA->QueryRegAddr(0xfe07) );		// fe07 (misc control)
			
			/* the rest are palette bytes */
			for(int palcount = 0xfe08; palcount < 0xfe0f; palcount++)
				SaveFile->CurrentChunk()->PutC( Disp->Read(palcount) );

			/* 4 bytes: 16 Mhz cycles since end of display */
			SaveFile->CurrentChunk()->Put32(0);

		/* WD1770 - needed only if the Plus 3 is enabled */
		if(CurrentConfig.Plus3.Enabled)
		{
		}
		
		/* memory - which parts to save depends on current memory configuration */
			/* save normal 32 kB */
			SaveFile->EstablishChunk();
			SaveFile->CurrentChunk()->SetId(0x0410);

			SaveFile->CurrentChunk()->PutC(0);			// 'update byte' - no updates!
			SaveFile->CurrentChunk()->PutC(0);			// standard RAM

			/* ram data itself */
			Uint8 MemoryBuffer[32768];
			CPU->ReadMemoryBlock(ULA->GetMappedAddr(MEM_RAM), 0, 32768, MemoryBuffer);
			SaveFile->CurrentChunk()->Write(MemoryBuffer, 32768);

			/* save shadow RAM if any is in use */
			if(CurrentConfig.MRBMode == MRB_SHADOW)
			{
				/* save 64 kB */
				Uint8 MemoryBuffer[32768];
				CPU->ReadMemoryBlock(ULA->GetMappedAddr(MEM_SHADOW), 0, 32768, MemoryBuffer);
				SaveFile->CurrentChunk()->Write(MemoryBuffer, 32768);
			}
			
		/* put Slogger MRB mode */
			SaveFile->EstablishChunk();
			SaveFile->CurrentChunk()->SetId(0x0420);
			
			SaveFile->CurrentChunk()->PutC(0);			// 'update byte' - no updates!
			switch(CurrentConfig.MRBMode)
			{
				default:			SaveFile->CurrentChunk()->PutC(0); break;
				case MRB_TURBO:
				case MRB_4Mhz:		SaveFile->CurrentChunk()->PutC(1); break;
				case MRB_SHADOW:	SaveFile->CurrentChunk()->PutC(2); break;
			}

	/* free UEF selector */
	GetHost() -> ReleaseUEFSelector(SaveFile);

	/* free locally allocated filename, if there is one */
	if(locname != fname)
		free(locname);

	fflush(stdout);

	ReleaseExclusivity();
	return true;
}

bool CProcessPool::MemoryDump(char *fname)
{
	FILE *dumpfile = fopen(fname, "wb");
	if(!dumpfile) return false;

	GetExclusivity();

		Uint8 MemoryBuffer[32768];
		CPU->ReadMemoryBlock(ULA->GetMappedAddr(MEM_RAM), 0, 32768, MemoryBuffer);
		fwrite(MemoryBuffer, 1, 32768, dumpfile);
		fclose(dumpfile);

	ReleaseExclusivity();
	return true;
}

bool CProcessPool::GetConfiguration(ElectronConfiguration &cfg)
{
	cfg = CurrentConfig;
	return true;
}

void CProcessPool::SetResetConfiguration()
{
	CurrentConfig = NextConfig;

	/* build component table. Always include CPU, display, ULA & tape */
	NumConnectedDevices = 4;
	ConnectedDevices[COMPONENT_CPU].Enabled = true;
	ConnectedDevices[COMPONENT_CPU].Component = CPU;

	ConnectedDevices[COMPONENT_ULA].Enabled = true;
	ConnectedDevices[COMPONENT_ULA].Component = ULA;

	ConnectedDevices[COMPONENT_DISPLAY].Enabled = true;
	ConnectedDevices[COMPONENT_DISPLAY].Component = Disp;

	ConnectedDevices[COMPONENT_TAPE].Enabled = true;
	ConnectedDevices[COMPONENT_TAPE].Component = Tape;

	/* add Plus 3 component to emulation if requested */
	if(CurrentConfig.Plus3.Enabled)
	{
		ConnectedDevices[NumConnectedDevices].Enabled = true;
		ConnectedDevices[NumConnectedDevices].Component = Disc;
		NumConnectedDevices++;
	}

	/* add Plus 1 component if requested */
	if(CurrentConfig.Plus1)
	{
		ConnectedDevices[NumConnectedDevices].Enabled = true;
		ConnectedDevices[NumConnectedDevices].Component = Plus1;
		NumConnectedDevices++;
	}

	/* clear all trap addresses */
	unsigned int c = NumTrapTables;
	while(c--)
		AllTrapTables[c].Clear();

	/* attach CPU and ULA */
	c = 2;
	while(c--)
		ConnectedDevices[c].Component->AttachTo(*this, c);

	/* pass on info for timing & memory mapping to ULA */
	ULA->SetMemoryModel(CurrentConfig.MRBMode);

	/* attach all other devices, now that memory is all set up */
	for(c = 2; c < NumConnectedDevices; c++)
		ConnectedDevices[c].Component->AttachTo(*this, c);

	// request relevant ROMs
	//if(CurrentConfig.Autoconfigure)
	{
		if(CurrentConfig.Plus3.Enabled) 
		{
			ROMHint(true, NULL, ROMTYPE_ADFS);
			ROMHint(true, NULL, ROMTYPE_DFS);
		}
		else
			ROMHint(false, NULL, ROMTYPE_FS);
	}
/*	else
	{
		if(CurrentConfig.Plus3.Enabled)
		{
			ROMHint(true, NULL, ROMTYPE_ADFS);
			ROMHint(true, NULL, ROMTYPE_DFS);
		}
	}*/

	/* follow instructions on Plus 1 ROM */
	if(CurrentConfig.Plus1)
		ULA->InstallROM("%ROMPATH%/plus1.rom", PLUS1_SLOT);
	else
		ULA->SetROMMode(PLUS1_SLOT, ROMMODE_OFF);
		
	/* map in additional requested ROMs */
}

void CProcessPool::SetNonResetConfiguration(ElectronConfiguration *NewCfg, bool force)
{
	/* things that can be done without a reset */
	NextConfig = *NewCfg;

	/* change memory model only if we're not in shadow or supposed to switch to shadow */
	if(CurrentConfig.MRBMode != MRB_SHADOW && NewCfg->MRBMode != MRB_SHADOW)
	{
		if(CurrentConfig.MRBMode != NewCfg->MRBMode)
		{
			CurrentConfig.MRBMode = NewCfg->MRBMode;
			ULA->SetMemoryModel(NewCfg->MRBMode);
		}
	}

	/* copy down the various things the ProcessPool can respond to right now */
	CurrentConfig.Autoload = NewCfg->Autoload;
	CurrentConfig.Autoconfigure = NewCfg->Autoconfigure;

	/* if state is out of sync, note so here so that it can be fixed on next reset */
	if(
		(CurrentConfig.MRBMode != NewCfg->MRBMode) ||
		(CurrentConfig.Plus1 != NewCfg->Plus1) ||
		(CurrentConfig.Plus3.Enabled != NewCfg->Plus3.Enabled)
	)
	{
		ConfigDirty = true;
	}
}

void CProcessPool::CreateTrapAddressSets(int num)
{
	delete[] AllTrapTables;
	CurrentTrapTable = AllTrapTables = new TrapTable[NumTrapTables = num];
}
void CProcessPool::SetTrapAddressSet(int num)
{
	CurrentTrapTable = &AllTrapTables[num];

	int c = NumConnectedDevices;
	while(c--)
		ConnectedDevices[c].Component->IOCtl(IOCTL_NEWTRAPFLAGS, (void *)CurrentTrapTable, TotalCycles);
}
bool CProcessPool::IsTrapAddress(Uint16 Addr) { return CurrentTrapTable->TrapAddrFlags[Addr >> 5]&(1 << (Addr&31)) ? true : false; }
bool CProcessPool::ClaimTrapAddressSet(Uint32 id, Uint16 Value, Uint16 Mask)
{
	/* thick interpretation */
	Uint32 BC = 0x10000;
	while(BC--)
	{
		if((BC&Mask) == Value)
		{
			CurrentTrapTable->TrapAddrFlags[BC >> 5] |= (1 << (BC&31));
			if(!CurrentTrapTable->TrapAddrDevices[BC >> 8])
			{
				CurrentTrapTable->TrapAddrDevices[BC >> 8] = new Uint32[256];
				memset(CurrentTrapTable->TrapAddrDevices[BC >> 8], 0, sizeof(Uint32)*256);
			}
			CurrentTrapTable->TrapAddrDevices[BC >> 8][BC & 0xff] = id;
		}
	}

	return true;
}
bool CProcessPool::ClaimTrapAddress(Uint32 id, Uint16 Value, Uint16 Mask)
{
	if(Mask != 0xffff)
	{
		/* thick interpretation */
		Uint32 BC = 0x10000;
		while(BC--)
		{
			if((BC&Mask) == Value)
			{
				int c = NumTrapTables;
				while(c--)
				{
					AllTrapTables[c].TrapAddrFlags[BC >> 5] |= (1 << (BC&31));
					if(!AllTrapTables[c].TrapAddrDevices[BC >> 8])
						AllTrapTables[c].TrapAddrDevices[BC >> 8] = new Uint32[256];
					AllTrapTables[c].TrapAddrDevices[BC >> 8][BC & 0xff] = id;
				}
			}
		}
	}
	else
	{
		int c = NumTrapTables;
		while(c--)
		{
			AllTrapTables[c].TrapAddrFlags[Value >> 5] |= (1 << (Value&31));
			if(!AllTrapTables[c].TrapAddrDevices[Value >> 8])
				AllTrapTables[c].TrapAddrDevices[Value >> 8] = new Uint32[256];
			AllTrapTables[c].TrapAddrDevices[Value >> 8][Value & 0xff] = id;
		}
	}

	return true;
}

bool CProcessPool::ReleaseTrapAddress(Uint32 id, Uint16 Value, Uint16 Mask)
{
	if(Mask != 0xffff)
	{
		/* v. thick interpretation */
		Uint32 BC = 0x10000;
		while(BC--)
		{
			if((BC&Mask) == Value)
			{
				int c = NumTrapTables;
				while(c--)
					AllTrapTables[c].TrapAddrFlags[BC >> 5] &= ~(1 << (BC&31));
			}
		}
	}
	else
	{
		int c = NumTrapTables;
		while(c--)
			AllTrapTables[c].TrapAddrFlags[Value >> 5] &= ~(1 << (Value&31));
	}

	return true;
}

CComponentBase *CProcessPool::GetWellDefinedComponent(Uint32 id)
{
	switch(id)
	{
		default: return ConnectedDevices[id].Component;
		case COMPONENT_PLUS1: return Plus1;
		case COMPONENT_PLUS3: return Disc;
	}
}

void CProcessPool::DebugMessage(Uint32 msg)
{
	if((DebugMask&msg) || !msg)
	{
		SDL_Event ev;
		ev.type = SDL_USEREVENT;
		ev.user.code = msg;
		SDL_PushEvent(&ev);

		/* an event that prevents any further processing? */
		switch(msg)
		{
			case PPDEBUG_USERQUIT:
			case PPDEBUG_SCREENFAILED:
				Quit = true;
			break;

			default: break;
		}
	}
}

void CProcessPool::Message(PPMessage msg, void *parameter)
{
	/* receives a message. This will be one of:

			- CPU encountered a KILL operation
			- CPU encountered an unknown operation
			- tape data has started loading
			- tape data has stopped loading
	*/

	switch(msg)
	{
		default: break;
		case PPM_TAPEDATA_TRANSIENT: InTape = true; InTapeTransient = 2; break;
		case PPM_TAPEDATA_START: InTape = true; InTapeTransient = 0; break;
		case PPM_TAPEDATA_STOP: if(InTape && !InTapeTransient) InTapeTransient = 2; break;

		case PPM_QUIT:
			Quit = true;
			DebugMessage(PPDEBUG_USERQUIT);
		break;
		case PPM_FSTOGGLE:
			DebugMessage(PPDEBUG_FSTOGGLE);
		break;

		case PPM_CPUDIED: DebugMessage(PPDEBUG_KILLINSTR); break;
		case PPM_UNKNOWNOP: DebugMessage(PPDEBUG_UNKNOWNOP); break;
		case PPM_GUI: DebugMessage(PPDEBUG_GUI); break;

		case PPM_HARDRESET:
			IOCtl(IOCTL_SUPERRESET);
		break;
	}
}

bool CProcessPool::IOCtl(Uint32 Control, void *Parameter, Uint32 TimeStamp)
{
	bool Handled = false;

	/* take any 'special' actions that flow from this IOCtl but don't affect how it is handled otherwise */
	switch(Control)
	{
		case IOCTL_SETIRQ:
			if(Parameter) DebugMessage(PPDEBUG_IRQ);
		break;
		case IOCTL_SETNMI:
			if(Parameter) DebugMessage(PPDEBUG_NMI);
		break;

		case IOCTL_SETRST:
			if(Parameter)
			{
		case IOCTL_RESET:
		case IOCTL_SUPERRESET:
				if(ConfigDirty)
				{
					GetExclusivity();
					SetResetConfiguration();
					ConfigDirty = false;
					int c = NumConnectedDevices;
					while(c--) ConnectedDevices[c].Component->IOCtl(IOCTL_SETCONFIG_RESET, &NextConfig, TimeStamp);
					ReleaseExclusivity();
				}				
			}
		break;

		default:break;
	}

	/* pass message on now if possible */
	switch(Control)
	{
		case PPOOLIOCTL_MUTEXWAIT:
		case PPOOLIOCTL_BREAK:
			SDL_mutexP(IOCtlMutex);
				IOCtlBuffer[IOCtlBufferWrite].Control = Control;
				IOCtlBuffer[IOCtlBufferWrite].TimeStamp = TimeStamp;
				IOCtlBuffer[IOCtlBufferWrite].Parameter = Parameter;

				IOCtlBufferWrite = (IOCtlBufferWrite+1)&(IOCTLBUFFER_LENGTH-1);

				if(Control == PPOOLIOCTL_MUTEXWAIT)
				{
					SDL_mutexP(RunningMutex); //grab running mutex so that when this message is processed, emulation will pause
				}
			SDL_mutexV(IOCtlMutex);
				if(Control == PPOOLIOCTL_MUTEXWAIT)
				{
					/* hang around for main thread to get to block */
					while(MainThreadRunning)
						SDL_Delay(10);
				}
			Handled = true;
		break;

		case IOCTL_SETCONFIG:
		{
			GetExclusivity();
			SetNonResetConfiguration((ElectronConfiguration *)Parameter);
			int c = NumConnectedDevices;
			while(c--) ConfigDirty |= ConnectedDevices[c].Component->IOCtl(Control, Parameter, TimeStamp);
			ReleaseExclusivity();
		}
		return true;

		case IOCTL_GETCONFIG:
			*(ElectronConfiguration *)Parameter = CurrentConfig;
			Handled = true;
		return true;

		case IOCTL_GETCONFIG_FINAL:
			*(ElectronConfiguration *)Parameter = NextConfig;
			Handled = true;
		return true;

		default:
		{
			int c = NumConnectedDevices;
			while(c--) Handled |= ConnectedDevices[c].Component->IOCtl(Control, Parameter, TimeStamp);
		}
		break;
	}

	return Handled;
}

void CProcessPool::SendInitialIOCtls()
{
	IOCtl(IOCTL_SETRST, NULL, TotalCycles);
	IOCtl(IOCTL_SETNMI, NULL, TotalCycles);
	IOCtl(IOCTL_SETIRQ, NULL, TotalCycles);
	IOCtl(IOCTL_NEWTRAPFLAGS, (void *)CurrentTrapTable, TotalCycles);
}
