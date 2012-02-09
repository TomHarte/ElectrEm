/*

	ElectrEm (c) 2000-6 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	Tape/FeederUEF.cpp
	==================

	UEF-Tape file reader. Converts a UEF into the correct input waves or bits.
	Supports the entire set of UEF defined tape chunks.

*/
#include "Internal.h"
#include "../HostMachine/HostMachine.h"

#define UEF_VERSION		0x000a

#define BAUDWISE_GAP	0x0112
#define FLOATING_GAP	0x0116
#define HTONE			0x0110
#define HTONEDUMMY		0x0111

#define PHASE_CHANGE	0x0115
#define BAUD_RATE		0x0113

#define IMPLICIT_DATA	0x0100
#define EXPLICIT_DATA	0x0102

#define SECURITY		0x0114
#define DEFINED_DATA	0x0104

#define MULTI_DATA1		0x0101
#define MULTI_DATA2		0x0103

class CUEFChunkFeeder
{
	public:
		void Setup(CUEFChunkSelector * = NULL, float BaudRate = 0, Uint16 Phase = 0);
		virtual ~CUEFChunkFeeder();

		virtual bool Initialise();
		virtual TapeWave ReadWave();
		virtual TapeBit ReadBit();
		virtual bool Finished();
		virtual bool BitFeeder();

		/* these trim off the requested number of cycles (or as close to as is possible) */
		virtual void TrimEnd(Uint32);
		virtual void TrimStart(Uint32);
		virtual Uint32 GetLength();

		struct Bit
		{
			Uint8 Value8; Uint32 Value32;
		};

	protected:
		virtual bool LFinished();
		void InitialiseDefaultFeeder();
		bool BFeeder;
		Uint16 Phase;
		float BaudRate;

		Bit CBit;
		int BitStage;
		Uint32 BitLength, PulseLength;

		Uint32 TimeOffset, BitLengthFixed, BitCount;

		virtual Bit GetBit();

		CUEFChunk *DChunk8, *DChunk32;
		CUEFChunkSelector *sel;
};

bool CUEFChunkFeeder::Initialise() { return true;}
bool CUEFChunkFeeder::BitFeeder() { return BFeeder; }
CUEFChunkFeeder::Bit CUEFChunkFeeder::GetBit()
{ 
	Bit r = {0}; 
	return r;
}
bool CUEFChunkFeeder::Finished() { return LFinished() && (BFeeder ?  true : !BitStage); }
bool CUEFChunkFeeder::LFinished() { return true; }
void CUEFChunkFeeder::Setup(CUEFChunkSelector *s, float BR, Uint16 Ph)
{
	BFeeder = false;
	BitStage = 0;

	BaudRate = BR;
	BitLength = (Uint32)(2000000.0f / BaudRate);
	Phase = Ph;

	TimeOffset = BitCount = 0;
	BitLengthFixed = (Uint32)((2000000.0f*65536.0f) / BaudRate);

	sel = s;
	DChunk8 = sel->GetChunkPtr();

	Uint32 Offset = sel->GetOffset();
	sel->Seek(1, SEEK_CUR);
	if(	(sel->CurrentChunk()->GetId() == MULTI_DATA1) ||
		(sel->CurrentChunk()->GetId() == MULTI_DATA2)
	)
		DChunk32 = sel->GetChunkPtr();
	else
	{
		DChunk32 = NULL;
		sel->Seek(Offset, SEEK_SET);
	}
}
CUEFChunkFeeder::~CUEFChunkFeeder()
{
	if(DChunk8) sel->ReleaseChunkPtr(DChunk8);
	if(DChunk32) sel->ReleaseChunkPtr(DChunk32);
}
void CUEFChunkFeeder::InitialiseDefaultFeeder()
{
	BFeeder = true;
}

TapeBit CUEFChunkFeeder::ReadBit()
{
	CBit = GetBit();

	TapeBit ret;

	/* time */
	ret.Length = (Uint32)((Uint16)(((TimeOffset+BitLengthFixed) >> 16) - (TimeOffset >> 16)));
	TimeOffset += BitLength;

	/* other stuff */
	ret.Type = TapeBit::DATA;
	ret.Value32 = CBit.Value32;
	ret.Value8 = CBit.Value8;
	return ret;
}

TapeWave CUEFChunkFeeder::ReadWave()
{
	TapeWave ret;
	static int BitAdd;

	if(!BitStage)
	{
		CBit = GetBit();

		PulseLength = CBit.Value8 ? (BitLengthFixed >> 2) : (BitLengthFixed >> 1);
		BitAdd = 2-CBit.Value8;

		BitStage = CBit.Value8 ? 4 : 2;
	}

	ret.Length =	(Uint32)((float)(BitCount+BitAdd) * (500000.0f / BaudRate)) -
					(Uint32)((float)BitCount * (500000.0f / BaudRate));
	BitCount += BitAdd;
//	ret.Length = (Uint32)((Uint16)(((TimeOffset+PulseLength) >> 16) - (TimeOffset >> 16)));
//	TimeOffset += PulseLength;

	ret.Type = (BitStage&1) ? TapeWave::HIGH : TapeWave::LOW;
	ret.Phase = Phase;
	BitStage--;

	return ret;
}

void CUEFChunkFeeder::TrimEnd(Uint32)
{
}

void CUEFChunkFeeder::TrimStart(Uint32 cycles)
{
}

Uint32 CUEFChunkFeeder::GetLength()
{
	return 0;
}

/*

	GAP (&0116 and &0112)

*/
class CUEFChunkFeederGap: public CUEFChunkFeeder
{
	public:
		CUEFChunkFeederGap(CUEFChunkSelector *a, float b, Uint16 c) { Setup(a, b, c); }

		bool Initialise();
		TapeWave ReadWave();

		void TrimEnd(Uint32);
		void TrimStart(Uint32);
		Uint32 GetLength();

	private:
		Uint32 Length;
};

bool CUEFChunkFeederGap::Initialise()
{
	if(DChunk8->GetId() == BAUDWISE_GAP)
	{
		Uint16 Len = DChunk8->Get16();
		Length = (Uint32)((1000000.0f / BaudRate) * (float)Len);
	}
	else
		Length = (Uint32)(DChunk8->GetFloat() * 2000000.0f);

	return true;
}

TapeWave CUEFChunkFeederGap::ReadWave()
{
	TapeWave r;

	r.Length = Length;
	r.Type = TapeWave::GAP;
	r.Phase = Phase;

	return r;
}

void CUEFChunkFeederGap::TrimEnd(Uint32 cycles)
{
	Length -= cycles;

	DChunk8->ReadSeek(0, SEEK_SET);
	if(DChunk8->GetId() == BAUDWISE_GAP)
	{
		Uint16 Len = (Uint16)(((float)Length * BaudRate) / 1000000.0f);
		DChunk8->Put16(Len);
	}
	else
	{
		float Len = (float)Length / 2000000.0f;
		DChunk8->PutFloat(Len);
	}
}

void CUEFChunkFeederGap::TrimStart(Uint32 cycles)
{
	/* these are equivalent, so... */
	TrimEnd(cycles);
}

Uint32 CUEFChunkFeederGap::GetLength()
{
	return Length;
}

/*

	SECURITY WAVES (&0114)

*/
class CUEFChunkFeederSecurity: public CUEFChunkFeeder
{
	public:
		CUEFChunkFeederSecurity(CUEFChunkSelector *a, float b, Uint16 c) { Setup(a, b, c); }

		bool Initialise();

		TapeWave ReadWave();
		bool Finished();
	private:
		Uint32 NumWaves;
		bool StartHalf, EndHalf;

		Uint8 Data;
		int ShiftPos;
		Uint32 WaveCount;
		bool New;
};

bool CUEFChunkFeederSecurity::Initialise()
{
	NumWaves = DChunk8->Get16();
	NumWaves |= DChunk8->GetC() << 16;

	StartHalf = (DChunk8->GetC() == 'P') ? true : false;
	EndHalf = (DChunk8->GetC() == 'P') ? true : false;

	Data = DChunk8->GetC();
	ShiftPos = 8;
	WaveCount = 0;
	New = true;

	return true;
}

TapeWave CUEFChunkFeederSecurity::ReadWave()
{
	TapeWave r;

	r.Length = (Data&1) ? (BitLength >> 2) : (BitLength >> 1);
	r.Phase = Phase;
	int type = 0;
	bool Advance = false;

	if(!WaveCount && StartHalf) type = 1;
	if((WaveCount == (NumWaves-1)) && EndHalf) type = 2;

	switch(type)
	{
		case 1:
			/* first wave - in scenario when second half only */
			r.Type = TapeWave::HIGH;
			Advance = true;
		break;

		case 2:
			/* final wave - in scenario when first half only */
			r.Type = TapeWave::LOW;
			Advance = true;
		break;

		default:
			/* any old ordinary wave */
			r.Type = New ? TapeWave::LOW : TapeWave::HIGH;
			Advance = New = !New;
		break;
	}

	if(Advance)
	{
		Data >>= 1;
		WaveCount++;
		ShiftPos--;
		if(!ShiftPos)
		{
			Data = DChunk8->GetC();
			ShiftPos = 8;
		}
	}

	return r;
}

bool CUEFChunkFeederSecurity::Finished()
{
	return WaveCount == NumWaves;
}

/*

	INLINE SNAPSHOT (&04xx - not really handled here, just passed on, eventually to process pool)

*/
class CUEFChunkFeederSnapshot: public CUEFChunkFeeder
{
	public:
		CUEFChunkFeederSnapshot(CUEFChunkSelector *a, float b, Uint16 c) { Setup(a, b, c); }
		TapeWave ReadWave();
};

TapeWave CUEFChunkFeederSnapshot::ReadWave()
{
	TapeWave r;

	r.Length = 0;
	r.Type = TapeWave::SNAPSHOTCK;
	r.SNChunk = DChunk8;
	r.Phase = Phase;

	return r;
}

/*

	HIGH TONE (&0110)

*/
class CUEFChunkFeederHTone: public CUEFChunkFeeder
{
	public:
		CUEFChunkFeederHTone(CUEFChunkSelector *a, float b, Uint16 c) { Setup(a, b, c); }
		bool Initialise();

		void TrimEnd(Uint32);
		void TrimStart(Uint32);
		Uint32 GetLength();
	private:
		Bit HighBit;
		Uint16 BitCount;
		Bit GetBit();

		bool LFinished();
};

bool CUEFChunkFeederHTone::Initialise()
{
	InitialiseDefaultFeeder();

	HighBit.Value32 = 0;
	HighBit.Value8 = 1;
	BitCount = DChunk8->Get16() >> 1; /* fix me */

	return true;
}

CUEFChunkFeederHTone::Bit CUEFChunkFeederHTone::GetBit()
{
	if(BitCount) BitCount--;
	return HighBit;
}

bool CUEFChunkFeederHTone::LFinished()
{
	return BitCount ? false : true;
}

void CUEFChunkFeederHTone::TrimEnd(Uint32 cycles)
{
	cycles -= cycles%BitLength;
	BitCount -= (Uint16)(cycles / BitLength);

	DChunk8->ReadSeek(0, SEEK_SET);
	Uint16 Len = (Uint16)(BitLength*BitCount);
	DChunk8->Put16(Len);
}

void CUEFChunkFeederHTone::TrimStart(Uint32 cycles)
{
	/* these are equivalent, so... */
	TrimEnd(cycles);
}

Uint32 CUEFChunkFeederHTone::GetLength()
{
	return BitCount*BitLength;
}
/*

	HIGH TONE feat. dummy (&0111)

*/
class CUEFChunkFeederHToneDummy: public CUEFChunkFeeder
{
	public:
		CUEFChunkFeederHToneDummy(CUEFChunkSelector *a, float b, Uint16 c) { Setup(a, b, c); }
		bool Initialise();
		Uint32 GetLength();

	private:
		Bit HighBit;
		Uint16 BitCount1, BitCount2, IBCount;
		Uint32 IBits;
		Bit GetBit();

		bool LFinished();
};

bool CUEFChunkFeederHToneDummy::Initialise()
{
	InitialiseDefaultFeeder();

	HighBit.Value32 = 0;
	HighBit.Value8 = 1;
	BitCount1 = sel->CurrentChunk()->Get16() >> 1; /* NB: fix me */
	BitCount2 = sel->CurrentChunk()->Get16() >> 1;
	IBCount = 10;
	IBits = ((0xaa) << 1) | 0x200;

	return true;
}

CUEFChunkFeederHToneDummy::Bit CUEFChunkFeederHToneDummy::GetBit()
{
	if(BitCount1)
	{
		BitCount1--;
		return HighBit;
	}

	if(IBits)
	{
		IBits--;
		Bit R;
		R.Value8 = (Uint8)(IBits&1);
		R.Value32 = R.Value8 ? 0xf : 0;
		IBits >>= 1;
		return R;
	}

	if(BitCount2) BitCount2--;
	return HighBit;
}

bool CUEFChunkFeederHToneDummy::LFinished()
{
	return BitCount2 ? false : true;
}

Uint32 CUEFChunkFeederHToneDummy::GetLength()
{
	return (BitCount1+BitCount2+10)*BitLength;
}

/*

	IMPLICIT DATA (&0100, possibly feat &0101/3)

*/
class CUEFChunkFeederImplicitData: public CUEFChunkFeeder
{
	public:
		bool Initialise();
		CUEFChunkFeederImplicitData(CUEFChunkSelector *a, float b, Uint16 c) { Setup(a, b, c); }
		Uint32 GetLength();

	private:
		int BitOffs;
		Uint8 Data8;
		Uint32 Data32;

		Bit GetBit();
		bool LFinished();
};

bool CUEFChunkFeederImplicitData::Initialise()
{
	InitialiseDefaultFeeder();

	Data8 = DChunk8->GetC();
	if(DChunk32) Data32 = DChunk32->Get32();
	BitOffs = 10;

	return true;
}

CUEFChunkFeederImplicitData::Bit CUEFChunkFeederImplicitData::GetBit()
{
	Bit NewBit;

	switch(BitOffs)
	{
		case 10: NewBit.Value8 = 0; NewBit.Value32 = 0; break;
		case 1: NewBit.Value8 = 1; NewBit.Value32 = 0xf; break;

		default:
			NewBit.Value8 = Data8&1;
			NewBit.Value32 = Data32&0xf;
			Data8 >>= 1;
			Data32 >>= 4;
		break;
	}
	BitOffs--;

	if(!BitOffs)
	{
		if(!DChunk8->EOC())
		{
			Data8 = DChunk8->GetC();
			if(DChunk32) Data32 = DChunk32->Get32();
			BitOffs = 10;
		}
	}

	return NewBit;
}

bool CUEFChunkFeederImplicitData::LFinished()
{
	return DChunk8->EOC() && !BitOffs;
}

Uint32 CUEFChunkFeederImplicitData::GetLength()
{
	return (Uint32)((float)DChunk8->GetLength() * (20000000.0f / BaudRate));
}

/*

	EXPLICIT DATA (&0102, possibly feat &0101/3)

*/
class CUEFChunkFeederExplicitData: public CUEFChunkFeeder
{
	public:
		bool Initialise();
		CUEFChunkFeederExplicitData(CUEFChunkSelector *a, float b, Uint16 c) { Setup(a, b, c); }
		Uint32 GetLength();

	private:
		int BitOffs;
		Uint8 Data8, EMask;
		Uint32 Data32;

		Bit GetBit();
		bool LFinished();
};

bool CUEFChunkFeederExplicitData::Initialise()
{
	InitialiseDefaultFeeder();

	EMask = DChunk8->GetC();
	if(DChunk32) DChunk32->ReadSeek(4, SEEK_CUR);

	Data8 = DChunk8->GetC();
	if(DChunk32) Data32 = DChunk32->Get32();
	BitOffs = 8;

	return true;
}

CUEFChunkFeederExplicitData::Bit CUEFChunkFeederExplicitData::GetBit()
{
	Bit NewBit;

	NewBit.Value8 = Data8&1;
	NewBit.Value32 = Data32&0xf;
	Data8 >>= 1;
	Data32 >>= 4;
	BitOffs--;

	if(!BitOffs)
	{
		if(!DChunk8->EOC())
		{
			Data8 = DChunk8->GetC();
			if(DChunk32) Data32 = DChunk32->Get32();
			BitOffs = 10;
		}
	}

	return NewBit;
}

bool CUEFChunkFeederExplicitData::LFinished()
{
	return DChunk8->EOC() && (BitOffs < EMask);
}

Uint32 CUEFChunkFeederExplicitData::GetLength()
{
	return ((DChunk8->GetLength() << 3) - EMask)*BitLength;
}

/*

	DEFINED DATA (&0104)

*/
class CUEFChunkFeederDefinedDataPulseWise: public CUEFChunkFeeder
{
	public:
		bool Initialise();
		CUEFChunkFeederDefinedDataPulseWise(CUEFChunkSelector *a, float b, Uint16 c) { Setup(a, b, c); }
		TapeWave ReadWave();
		bool Finished();
		Uint32 GetLength();

	private:
		int PulseOffs, DataOffs;
		Uint32 NumBytes;
		Uint16 Data8;
		Uint32 Data32;

		Uint8 BitsPerByte;
		bool ParityPresent;
		Uint8 ParityMask;
		Sint8 StopWaves;

		Uint8 Parity;
};

bool CUEFChunkFeederDefinedDataPulseWise::Initialise()
{
	BitsPerByte = DChunk8->GetC();
	switch(DChunk8->GetC())
	{
		case 'N': ParityPresent = false; break;
		case 'O': ParityPresent = true; ParityMask = 1; break; /* are these the right way around? */
		case 'E': ParityPresent = true; ParityMask = 0; break;
	}
	if(ParityPresent) BitsPerByte++;
	StopWaves = (signed)DChunk8->GetC();
	if(StopWaves > 0)
		StopWaves <<= 1;
	else
		StopWaves =- StopWaves;
	
	PulseOffs = DataOffs = 0;
	Data8 = 0;
	Data32 = 0;
	NumBytes = DChunk8->GetLength() - 3;;

	return true;
}

TapeWave CUEFChunkFeederDefinedDataPulseWise::ReadWave()
{
	TapeWave NewWave;
	NewWave.Phase = Phase;

	// start bit
	if(!DataOffs)
	{
		NewWave.Length = BitLength >> 1;
		switch(PulseOffs)
		{
			case 0: NewWave.Type = TapeWave::LOW; break;
			case 1: NewWave.Type = TapeWave::HIGH; break;
		}

		PulseOffs++;
		if(PulseOffs == 2)
		{
			Data8 = DChunk8->GetC();
			if(DChunk32) Data32 = DChunk32->Get32();
			DataOffs++; PulseOffs = 0;

			/* figure out parity */
			Parity = (Uint8)Data8;
			Parity ^= Parity >> 4;
			Parity ^= Parity >> 2;
			Parity ^= Parity >> 1;
			Parity ^= ParityMask;

			Data8 |= (Parity&1) << (BitsPerByte - 1);
		}

		return NewWave;
	}

	// data bits + parity (which has been appended)
	if(DataOffs < BitsPerByte)
	{
		bool NewBit = false;

		if(Data8&1)
		{
			NewWave.Length = BitLength >> 2;
			switch(PulseOffs)
			{
				case 2:
				case 0: NewWave.Type = TapeWave::LOW; break;

				case 3:
				case 1: NewWave.Type = TapeWave::HIGH; break;
			}

			PulseOffs++;
			if(PulseOffs == 4) NewBit = true;
		}
		else
		{
			NewWave.Length = BitLength >> 1;
			switch(PulseOffs)
			{
				case 0: NewWave.Type = TapeWave::LOW; break;
				case 1: NewWave.Type = TapeWave::HIGH; break;
			}

			PulseOffs++;
  			if(PulseOffs == 2) NewBit = true;
		}

		if(NewBit)
		{
			DataOffs++; PulseOffs = 0;
			Parity ^= (Data8&1);
			Data8 >>= 1;
			Data32 >>= 4;
		}

		return NewWave;
	}

	// stop waves
	if(DataOffs == BitsPerByte)
	{
		NewWave.Length = BitLength >> 2;
		switch(PulseOffs&1)
		{
			case 0: NewWave.Type = TapeWave::LOW; break;
			case 1: NewWave.Type = TapeWave::HIGH; break;
		}

		PulseOffs++;
		if(PulseOffs == (StopWaves << 1))
		{
			PulseOffs = DataOffs = 0;
			NumBytes--;
		}

		return NewWave;
	}

	// code never gets here, but it stops MSVC complaining
	NewWave.Length = 0;
	return NewWave;
}

bool CUEFChunkFeederDefinedDataPulseWise::Finished()
{
	return NumBytes ? false : true;
}

Uint32 CUEFChunkFeederDefinedDataPulseWise::GetLength()
{
	return (NumBytes*(((BitsPerByte+1 + (ParityPresent ? 1 : 0)) << 1) + StopWaves)*BitLength) >> 1;
}

/*

	Feeder proper

*/
CTapeFeederUEF::CTapeFeederUEF()
{
	TSelector = NULL;
	CSource = NULL;
}

CTapeFeederUEF::~CTapeFeederUEF()
{
	Close();
}

bool CTapeFeederUEF::Open(char *name)
{
	if(TSelector = GetHost() -> GetUEFSelector(name, UEF_VERSION))
	{
		if(TSelector->FindIdMajor(0x01))
		{
			/* default settings for modal values */
			BaudRate = 1200;
			Phase = 180;

			GetNewSource();

			return true;
		}
	}

	return false;
}

void CTapeFeederUEF::Close()
{
	if(CSource)
	{
		delete CSource;
		CSource = NULL;
	}

	if(TSelector)
	{
		GetHost() -> ReleaseUEFSelector(TSelector);
		TSelector = NULL;
	}
}

Uint32 ChunkLength, ChLength;
Uint16 OldID;

void CTapeFeederUEF::GetNewSource()
{
	/* delete current source, if relevant */
	if(CSource)
	{
		delete CSource; CSource = NULL;
	}
	Waves = 0;
	 
	/* find next useful chunk */
	while(!CSource)
	{
		TSelector->Seek(1, SEEK_CUR);

#ifdef DUMP_CHUNKS
		printf("Chunk %04x\n", TSelector->CurrentChunk()->GetId());
#endif
		Uint16 id = TSelector->CurrentChunk()->GetId();
		switch(id)
		{
			case BAUDWISE_GAP:
			case FLOATING_GAP:
				CSource = new CUEFChunkFeederGap(TSelector, BaudRate, Phase);
			break;

			case HTONE:
				CSource = new CUEFChunkFeederHTone(TSelector, BaudRate, Phase);
			break;
			case HTONEDUMMY:
				CSource = new CUEFChunkFeederHToneDummy(TSelector, BaudRate, Phase);
			break;

			case IMPLICIT_DATA:
				CSource = new CUEFChunkFeederImplicitData(TSelector, BaudRate, Phase);
			break;
			case EXPLICIT_DATA:
				CSource = new CUEFChunkFeederExplicitData(TSelector, BaudRate, Phase);
			break;
			case SECURITY:
				CSource = new CUEFChunkFeederSecurity(TSelector, BaudRate, Phase);
			break;
			case DEFINED_DATA:
				/* NB: is this one that is going to have to be pulsed? */
				CSource = new CUEFChunkFeederDefinedDataPulseWise(TSelector, BaudRate, Phase);
			break;

			case PHASE_CHANGE:	Phase = TSelector->CurrentChunk()->Get16();			break;
			case BAUD_RATE:		BaudRate = TSelector->CurrentChunk()->GetFloat();	break;

			default:
				/* inline snapshot? */
				if((id >> 8) == 0x4)
					CSource = new CUEFChunkFeederSnapshot(TSelector, BaudRate, Phase);
			break;
		}

		if(CSource)
			if(!CSource->Initialise())
			{
				delete CSource;
				CSource = NULL;
			}
			else
			{
				ChunkLength = CSource->GetLength();
				ChLength = 0;
				OldID = id;
			}
	}
}

TapeWave CTapeFeederUEF::ReadWave()
{
	Waves++;
	TapeWave w = CSource->ReadWave();
	ChLength += w.Length;
	if(CSource->Finished())
		GetNewSource();
	return w;
}

TapeBit CTapeFeederUEF::ReadBit()
{
	if(CSource->BitFeeder())
	{
		TapeBit NewBit = CSource->ReadBit();
		ChLength += NewBit.Length;
		Waves += NewBit.Value8 ? 4 : 2;

		if(CSource->Finished())
			GetNewSource();

		return NewBit;
	}
	else
		return CTapeFeeder::ReadBit();
}

Uint64 CTapeFeederUEF::Tell()
{
	return (TSelector->GetOffset()+1) | ((Uint64)Waves << 32);
}

void CTapeFeederUEF::Seek(Uint64 pos)
{
	Uint32 FilePos, WavePos;
	FilePos = (Uint32)(pos&0xffffffff);
	WavePos = (Uint32)(pos >> 32);

	if(!FilePos)
		TSelector->Reset();
	else
		TSelector->Seek(FilePos-1, SEEK_SET);

	GetNewSource();
	while(WavePos--)
		ReadWave();
}

bool CTapeFeederUEF::OverRan()
{
	return TSelector->OverRan();
}

void CTapeFeederUEF::ResetOverRan()
{
	TSelector->ResetOverRan();
}

Uint32 CTapeFeederUEF::GetLength()
{
	Uint64 StorePos = Tell();

	TSelector->Reset();
	TSelector->ResetOverRan();
	Uint32 Length = 0;

	while(!TSelector->OverRan())
	{
		GetNewSource();
		Length += CSource->GetLength();
	}

	Seek(StorePos);
	return Length;
}
