/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	Tape.cpp
	========

	Tape hardware emulation

*/

#include "Tape.h"
#include "Internal.h"
#include "../ULA.h"
#include "../ProcessPool.h"
#include "../6502.h"
#include "../HostMachine/HostMachine.h"

#include <string.h>
#include <ctype.h>

//#define DUMP_BITSTREAM

void CTape::AttachTo(CProcessPool &pool, Uint32 id)
{
	CComponentBase::AttachTo(pool, id);

	/* data register */
	pool.ClaimTrapAddress(id, 0xfe04, 0xff0f);


	TotalTime = RunTime = 0;
}

void CTape::AdvanceBit()
{
	if(Feeder)
	{
		switch(CurrentBit.Type)
		{
			default:break;

			case TapeBit::GAP:
				Silence = true;
			break;

			case TapeBit::DATA:
				Silence = false;
				ScrollRegister8 = (ScrollRegister8 >> 1) | (CurrentBit.Value8 << 11);
				ScrollRegister32 = (ScrollRegister32 >> 4) | ((Uint64)CurrentBit.Value32 << 36);
			break;
		}

		while(1)
		{
			CurrentBit = Feeder->ReadBit();
			if(CurrentBit.Type == TapeBit::SNAPSHOTCK)
				PPPtr->EffectChunk(CurrentBit.SNChunk);
			else
				break;
		}

#ifdef DUMP_BITSTREAM
		static int lc = 80;
		if(CurrentBit.Type == TapeBit::GAP)
		{
			printf("\n[gap %d]\n", CurrentBit.Length);
			lc = 80;
		}
		else
		{
			printf("%d", CurrentBit.Value8);
			lc--;
			if(!lc)
			{
				lc = 80;
				printf("\n");
			}
		}
#endif
	}
}

void CTape::RunTo(Uint32 Time)
{
	Uint32 Cycles = Time - RunTime;
	RunTime = Time;
	
	/*
		NB: Thanks to Tom Walker for making this abundantly obvious to me:
		
		Tape register empty and tape register full detection both always occur while the tape hardware is running,
		i.e. the tape hardware on "input" mode will still generate tape data empty, and on "output" mode will still
		generate tape data full...
	*/

	switch(CurrentMode)
	{
		default: break;

		case TM_INPUT:
			if(TapeMotor && !UseFastHack)
			{
				PPPtr->Message(PPM_TAPEDATA_START, NULL);

				CurrentBit.Length -= Cycles;

				if(!CurrentBit.Length)
				{
					AdvanceBit();

					/* any meaningful interrupts? */
					if(!Silence)
					{
							/* output interrupt */
/*							OutBitCount--;
							if(!OutBitCount)
							{
								((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, 0xff, ULAIRQ_SEND);
								printf("send [%d]\n", RunTime);
								OutBitCount = 9;
							}
							else
								printf("[[%d]]\n", OutBitCount);*/

						/* input bitcount */
						if(BitCount)
						{
							BitCount--;
							if(BitCount == 7)
								((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, ~ULAIRQ_RECEIVE, 0);
						}
						else
						{
							/* input interrupt */
							if((ScrollRegister8&0x3) == 0x1)
							{
								/* data interrupt */
								((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, ~ULAIRQ_HTONE, ULAIRQ_RECEIVE);
#ifdef DUMP_BITSTREAM
								fprintf(stderr, "%d\n", ScrollRegister8 >> 2);
#endif
								BitCount = 9;
							}

							if((ScrollRegister8&0x3ff) == 0x3ff)
							{
								/* high tone interrupt */
								((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, 0xff, ULAIRQ_HTONE);
#ifdef DUMP_BITSTREAM
								fprintf(stderr, ".", ScrollRegister8 >> 2);
#endif
							}
						}
					}
				}
			}
		break;

		case TM_OUTPUT:

			if(OutputCounter) OutputCounter -= Cycles;

			if(!OutputCounter)
			{
				OutputCounter = BIT_LENGTH;

				OutBitCount--;
				if(!OutBitCount)
				{
					((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, 0xff, ULAIRQ_SEND);
					OutBitCount = 9;
				}

				if(BitCount > 0)
				{
					BitCount--;
					if(BitCount == 7)
						((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, ~ULAIRQ_RECEIVE, 0);
				}
				else
				{
					if((ScrollRegister8&0x3) == 0x1)
					{
						/* data interrupt */
						((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, ~ULAIRQ_HTONE, ULAIRQ_RECEIVE);
						BitCount = 9;
					}

					if((ScrollRegister8&0x3ff) == 0x3ff)
					{
						/* high tone interrupt */
						((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, 0xff, ULAIRQ_HTONE);
					}
				}
				ScrollRegister8 = (ScrollRegister8 >> 1) | 0x200;
			}
			else
			{
//				((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, ~ULAIRQ_SEND, 0);
			}
		break;
	}
}

bool CTape::Write(Uint16 Addr, Uint32 TimeStamp, Uint8 Data8, Uint32 Data32)
{
	/*	General notes!
	
		trap addresses for filing system calls are claimed whenever those addresses
		fall within the OS ROM region. This means that both tape and ROM FS filing
		system addresses are claimed. Need to implement a check on all attached ROMs
		and not allow fast tape if a ROM with FS is attached
	
	*/
	RunTo(TimeStamp);

	switch(Addr&0xf)
	{
		case 0x04:
			if(CurrentMode == TM_OUTPUT)
			{
				ScrollRegister8 = (Data8 << 2) | 1;
				ScrollRegister32 = (Data32 << 8) | 0xf;
				BitCount = 0;
				if(OutputCounter < BIT_LENGTH)
				{
					OutputCounter = BIT_LENGTH;
					OutBitCount = 9;
					((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TimeStamp, ~ULAIRQ_SEND, 0);
				}
				return true;
			}
		break;
	}

	if(Addr < 0x8000)
	{
		C6502State CPUState;

		((C6502 *)PPPtr->GetWellDefinedComponent(COMPONENT_CPU))->GetState(CPUState);
		((C6502 *)PPPtr->GetWellDefinedComponent(COMPONENT_CPU))->WriteMem(CPUState.pc.a, Addr, Data8, Data32);
	}

	return false;
}

Uint32 CTape::Update(Uint32 Cycles, bool Catchup)
{
	TotalTime += Cycles;

	if(!TapeMotor || (CurrentMode == TM_OFF))
		PPPtr->Message(PPM_TAPEDATA_STOP, NULL);

	RunTo(TotalTime);

	if(TapeMotor && (CurrentMode == TM_INPUT))
		return CurrentBit.Length;

	if(CurrentMode == TM_OUTPUT)
	{
		if(OutputCounter)
			return OutputCounter;
		else
			return BIT_LENGTH;
	}

	return CYCLENO_ANY;
}

bool CTape::SetMode(Uint32 TimeStamp, TapeModes NM, bool NTM)
{
	RunTo(TimeStamp);
	
	if(NM == TM_OUTPUT)
	{
		OutputCounter = (TimeStamp-RunTime) + BIT_LENGTH;
	}
	
	if(NM == TM_INPUT)
	{
		((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, 0xff, ULAIRQ_SEND);
	}

	if((NM == TM_INPUT) && (CurrentMode != TM_INPUT))
		BitCount = 0;

	if(NM != TM_INPUT)
		((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, ~ULAIRQ_RECEIVE, 0);
	CurrentMode = NM;
	TapeMotor = NTM;

	return true;
}

void CTape::Close()
{
	if(Feeder)
	{
		delete Feeder;
		Feeder = NULL;
	}
}

bool CTape::Open(char *name)
{
	CTapeFeeder *OFeeder = Feeder;

	char *UEFExts[] = { "uef\a", NULL };
	if(GetHost() -> ExtensionIncluded(name, UEFExts))
	{
		CTapeFeeder *NewFeeder = new CTapeFeederUEF;
		if(!NewFeeder->Open(name))
		{
			delete NewFeeder;
			return false;
		}

		Close();
		Feeder = NewFeeder;
	}

	char *CSWExts[] = { "csw\a", NULL };
	if(GetHost() -> ExtensionIncluded(name, CSWExts))
	{
#ifdef WIN32	

		CTapeFeeder *NewFeeder = new CTapeFeederCSW;
		if(!NewFeeder->Open(name))
		{
			delete NewFeeder;
			return false;
		}

		Close();
		Feeder = NewFeeder;
#else
		return false; // can't do CSW on this platform!
#endif
	}

	if(Feeder != OFeeder)
	{
		StartPos = Feeder->Tell();
		DetermineType();
		CurrentBit = Feeder->ReadBit();
		return true;
	}

	return false;
}

CTape::CTape(ElectronConfiguration &cfg)
{
	CurrentMode = TM_OFF;
	Feeder = NULL;

	OutputCounter = BitCount = ScrollRegister8 = 0;
	Silence = true;
	UseFastHack = false;
}

CTape::~CTape()
{
}

bool CTape::IOCtl(Uint32 Control, void *Parameter, Uint32 TimeStamp)
{
	switch(Control)
	{
		case IOCTL_SETCONFIG_RESET:
		case IOCTL_SETCONFIG:
			UseFastHack = ((ElectronConfiguration *)Parameter)->FastTape;
			SetTrapAddresses(UseFastHack, ((ElectronConfiguration *)Parameter)->MRBMode == MRB_SHADOW);
		return false;

		case TAPEIOCTL_REWIND:
			if(Feeder)
				Feeder->Seek(StartPos);
		return true;
	}

	return CComponentBase::IOCtl(Control, Parameter, TimeStamp);
}

const char *CTape::QueryTapeCommand()
{
	static char RunCmd[] = "*TAPE\n*RUN\n";
	static char ChainCmd[] = "*TAPE\nCHAIN\"\"\n";

	switch(LoadType)
	{
		case LT_RUN: return RunCmd;
		case LT_CHAIN: return ChainCmd;
		default: return NULL;
	}
}
