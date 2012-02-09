/*

	ElectrEm (c) 2000-6 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	FastTape.cpp
	============

	Fast tape hack. Loads things unrealistically quickly.

	Fast notes:

		F359 in BBC ROM appears to be 'load block' ?

		F884 is get byte? (F7e5 in elk)


		Seems:
		
		&c0 is 'status byte' - goes to &80 when a new byte has been read
		&bd is where the next tape byte is stored

		OSFILE: f1d6

		-> f0fb load file

BBC ROM Position		OS100 Position		OS300 Position
f58d					f4e5*				f512
f77e					f6de				f70b
f79a					f6fa**				f727
fae0					fa51				fa79
f168					f0a8				f091
offer paged rom service	ea9e				eaa1

* implies f596 -> f4ee
** implies f7ad -> f70d

ee51 -> eaa6 (???)

ea9e: offer paged rom service - respond with a byte here

I currently put double-NOP at F58D, F77E, F79A and FAE0,
catch service call 14, and serve it if the tape filing
system is selected (?&247 == 0). I.e. I (ab)use the fact
that the tape and ROM filing system are intertwined.

Under emulation I suppose you could easily feed the system
'tape bytes' at the same hook points.
*/

#include "Tape.h"
#include "../ULA.h"
#include "../ProcessPool.h"
#include "../6502.h"

#include <string.h>
#include <ctype.h>

void CTape::SetTrapAddresses(bool Enabled, bool Slogger64)
{
	/* unclaim all */
	
		/* 32 kB ROM */
			PPPtr->ReleaseTrapAddress(PPNum, 0xf4e5);	PPPtr->ReleaseTrapAddress(PPNum, 0xf4e6);
			PPPtr->ReleaseTrapAddress(PPNum, 0xf6de);	PPPtr->ReleaseTrapAddress(PPNum, 0xf6df);
			PPPtr->ReleaseTrapAddress(PPNum, 0xf6fa);	PPPtr->ReleaseTrapAddress(PPNum, 0xf6fb);
			PPPtr->ReleaseTrapAddress(PPNum, 0xfa51);	PPPtr->ReleaseTrapAddress(PPNum, 0xfa52);

			PPPtr->ReleaseTrapAddress(PPNum, 0xf0a8);

		/* 64 kB ROM */
			PPPtr->ReleaseTrapAddress(PPNum, 0xf512);	PPPtr->ReleaseTrapAddress(PPNum, 0xf513);
			PPPtr->ReleaseTrapAddress(PPNum, 0xf70b);	PPPtr->ReleaseTrapAddress(PPNum, 0xf70c);
			PPPtr->ReleaseTrapAddress(PPNum, 0xf727);	PPPtr->ReleaseTrapAddress(PPNum, 0xf728);
			PPPtr->ReleaseTrapAddress(PPNum, 0xfa79);	PPPtr->ReleaseTrapAddress(PPNum, 0xfa7a);

			PPPtr->ReleaseTrapAddress(PPNum, 0xf091);

	if(Enabled)
	{
		/* claim */
		if(Slogger64)
		{
			PPPtr->ClaimTrapAddress(PPNum, 0xf512);		PPPtr->ClaimTrapAddress(PPNum, 0xf513);
			PPPtr->ClaimTrapAddress(PPNum, 0xf70b);		PPPtr->ClaimTrapAddress(PPNum, 0xf70c);
			PPPtr->ClaimTrapAddress(PPNum, 0xf727);		PPPtr->ClaimTrapAddress(PPNum, 0xf728);
			PPPtr->ClaimTrapAddress(PPNum, 0xfa79);		PPPtr->ClaimTrapAddress(PPNum, 0xfa7a);

			PPPtr->ClaimTrapAddress(PPNum, 0xf091);
		}
		else
		{
			PPPtr->ClaimTrapAddress(PPNum, 0xf4e5);		PPPtr->ClaimTrapAddress(PPNum, 0xf4e6);
			PPPtr->ClaimTrapAddress(PPNum, 0xf6de);		PPPtr->ClaimTrapAddress(PPNum, 0xf6df);
			PPPtr->ClaimTrapAddress(PPNum, 0xf6fa);		PPPtr->ClaimTrapAddress(PPNum, 0xf6fb);
			PPPtr->ClaimTrapAddress(PPNum, 0xfa51);		PPPtr->ClaimTrapAddress(PPNum, 0xfa52);

			PPPtr->ClaimTrapAddress(PPNum, 0xf0a8);
		}
	}
}
//#define DUMP_FASTACTION

void CTape::TapeGetC(Uint8 &Data8, Uint32 &Data32)
{
	int BitCount = 9;
	while(1)
	{
		AdvanceBit();

		if(!Silence)
		{
			if(BitCount)
			{
				BitCount--;
			}
			else
			{
				if((ScrollRegister8&0x3) == 0x1)
				{
					Data8 = (Uint8)(ScrollRegister8 >> 2);
					Data32 = (Uint32)(ScrollRegister32 >> 8);
					return;
				}
			}
		}
		else
			BitCount = 9;
	}
}

bool CTape::GetBlock(bool data)
{
	int Overruns = 0;
	while((Overruns < 2))
	{
		Feeder->ResetOverRan();

		/* seek high tone */
		while(((ScrollRegister8&0x3ff) != 0x3ff) && !Feeder->OverRan()) AdvanceBit();

		/* seek beyond high tone */
		while(((ScrollRegister8&0x3ff) == 0x3ff) && !Feeder->OverRan()) AdvanceBit();

		/* seek synchronisation byte, but quit early if new high tone appears */
		while(((ScrollRegister8&0x3ff) != 0x3ff) && ((ScrollRegister8&0x3ff) != 0x0a9) && !Feeder->OverRan()) AdvanceBit();

		/* found next block? */
		if((ScrollRegister8&0x3ff) == 0x0a9)
			break;

		if(Feeder->OverRan())
		{
			Overruns++;
			if(Overruns == 2)
				return false;
		}
	}

	/* read header */
	Uint8 Byte;
	Uint32 DWord;
	int Ptr = 0;

	/* name (including terminator) */
	while(1)
	{
		TapeGetC(Byte, DWord);

		TapeBlock.Name[Ptr] = Byte;
		Ptr++;
		if(!Byte || (Ptr == 11)) break;
	}
	TapeBlock.Name[10] = '\0';

	/* load address */
	TapeGetC(Byte, DWord); TapeBlock.LoadAddress = Byte;
	TapeGetC(Byte, DWord); TapeBlock.LoadAddress |= Byte << 8;
	TapeGetC(Byte, DWord); TapeBlock.LoadAddress |= Byte << 16;
	TapeGetC(Byte, DWord); TapeBlock.LoadAddress |= Byte << 24;

	/* exec address */
	TapeGetC(Byte, DWord); TapeBlock.ExecAddress = Byte;
	TapeGetC(Byte, DWord); TapeBlock.ExecAddress |= Byte << 8;
	TapeGetC(Byte, DWord); TapeBlock.ExecAddress |= Byte << 16;
	TapeGetC(Byte, DWord); TapeBlock.ExecAddress |= Byte << 24;

	/* block number */
	TapeGetC(Byte, DWord); TapeBlock.BlockNo = Byte;
	TapeGetC(Byte, DWord); TapeBlock.BlockNo |= Byte << 8;

	/* block length */
	TapeGetC(Byte, DWord); TapeBlock.BlockLength = Byte;
	TapeGetC(Byte, DWord); TapeBlock.BlockLength |= Byte << 8;

	/* block flags */
	TapeGetC(TapeBlock.Flags, DWord);

	/* four spare bytes */
	TapeGetC(Byte, DWord);
	TapeGetC(Byte, DWord);
	TapeGetC(Byte, DWord);
	TapeGetC(Byte, DWord);

	/* CRC */
	TapeGetC(Byte, DWord);
	TapeGetC(Byte, DWord);

	if(data)
	{
		/* Block body */
		for(int c = 0; c < TapeBlock.BlockLength; c++)
			TapeGetC(TapeBlock.Data8[c], TapeBlock.Data32[c]);
	}

#ifdef DUMP_FASTACTION
	fprintf(stderr, "block: %s %02x [l:%04x]\n", TapeBlock.Name, TapeBlock.BlockNo, TapeBlock.LoadAddress);
#endif

	return true;
}

bool CTape::Read(Uint16 Addr, Uint32 TimeStamp, Uint8 &Data8, Uint32 &Data32)
{
	C6502State CPUState;
	C6502 *CPU = (C6502 *)PPPtr->GetWellDefinedComponent(COMPONENT_CPU);
	static Uint32 LTimeStamp = 0;

	CPU->GetState(CPUState);

	if((Addr >= 0xc000) && CPUState.pc.a == Addr)
	{
		if(UseFastHack && Feeder && TapeHasROMData)
		{
			switch(Addr)
			{
				case 0xeaa1:
				case 0xea9e:
					Data8 = 0x80;
				return false; //immediate NOP
				case 0xf0a8:
				case 0xf091:
				{
					Uint8 T8;
					Uint32 T32;
					static bool LastData = false;
					CPU->ReadMem(CPUState.pc.a, 0x247, T8, T32);
					if(CPUState.x8 == 0xe && !T8) //check service call &14 is requested while filing system 0 (tape) is selected
					{
						// tape data is requested, so notify processpool that some transient fast processing is desirable
						PPPtr->Message(PPM_TAPEDATA_TRANSIENT, NULL);

						/* find start of new block if it has been more than half a second since anything was read */
						LTimeStamp = TimeStamp - LTimeStamp;
						if(LTimeStamp > 1000000)
						{
							while(1)
							{
								/* seek high tone */
								while((ScrollRegister8&0x3ff) != 0x3ff) AdvanceBit();

								/* seek beyond high tone */
								while((ScrollRegister8&0x3ff) == 0x3ff) AdvanceBit();
								
								int c = 8;
								while(c--) AdvanceBit();
								CPUState.y8 = (Uint8)(ScrollRegister8 >> 2);
								CPUState.y32 = (Uint32)(ScrollRegister32 >> 8);

								if(CPUState.y8 == 0x2a)
									break;
							}
						}
						else
						{
							// read a byte of tape data. TODO: FIX ME!
							do
							{
								TapeGetC(CPUState.y8, CPUState.y32);
								if(LastData) break;
							}
							while((ScrollRegister8&0xc00) != 0x400);
						}
						LTimeStamp = TimeStamp;

						LastData = (ScrollRegister8&0xc00) == 0x400;
						CPUState.a8 = 0;			/* set service call as claimed */
						CPU->SetState(CPUState);

						Data8 = 0x60; return false; //RTS
					}
				}
				break;
				default: Data8 = 0xea; return false; //NOP
			}
		}

		CPU->ReadMem(CPUState.pc.a, Addr, Data8, Data32);
		return false;
	}

	RunTo(TimeStamp);
	switch(Addr)
	{
		/* catch tape data read register */
		case 0xfe04:	case 0xfe14:	case 0xfe24:	case 0xfe34:
		case 0xfe44:	case 0xfe54:	case 0xfe64:	case 0xfe74:
		case 0xfe84:	case 0xfe94:	case 0xfea4:	case 0xfeb4:
		case 0xfec4:	case 0xfed4:	case 0xfee4:	case 0xfef4:
			Data8 = (Uint8)(ScrollRegister8 >> 2);
			Data32 = (Uint32)(ScrollRegister32 >> 8);
			((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TimeStamp, ~ULAIRQ_RECEIVE, 0);
//			((CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA))->AdjustInterrupts(TotalTime, 0xff, ULAIRQ_SEND);
		break;
		
		default:	//default: act as if nothing is special about this address
			CPU->ReadMem(CPUState.pc.a, Addr, Data8, Data32);
		break;
	}

	return false;
}

void CTape::DetermineType()
{
	/* check if first block looks like BASIC */
	if(TapeHasROMData = GetBlock())
	{
		LoadType = LT_CHAIN;

		// check if it looks like a BASIC program
		int Length = TapeBlock.BlockLength;
		Uint8 *TPos = TapeBlock.Data8;
		while(Length > 4)
		{
			if(TPos[0] != 13) {LoadType = LT_RUN; break; }
			if((TPos[1]&0x7f) == 0x7f) break;

			Length -= TPos[3];
			TPos += TPos[3];
		}
	}
	else
		LoadType = LT_UNKNOWN;

	Feeder->Seek(StartPos);
}
