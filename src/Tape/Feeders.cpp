#include "Internal.h"
#include <math.h>

/* these are 1200 baud +- 40% */

#undef ONE_WIDE_MAX
#undef ZERO_THIN_MIN

#define ONE_WIDE_MIN	250
#define ONE				417
#define ONE_THIN_MAX	550
#define ONE_WIDE_MAX	600

#define ZERO_WIDE_MIN	490
#define ZERO_THIN_MIN	680
#define ZERO			833
#define ZERO_WIDE_MAX	1100

#define WAVE_MIN	1250
#define WAVE_MAX	2100

//#define SHOWLOGIC

/* default readbit - constructs a bit from ReadWave */
CTapeFeeder::CTapeFeeder()
{
	InWavePos = 0;
}

CTapeFeeder::~CTapeFeeder() {}
void CTapeFeeder::ShiftWavesLeft(int positions)
{
	while(positions--)
	{
		InWaves[0] = InWaves[1];
		InWaves[1] = InWaves[2];
		InWaves[2] = InWaves[3];
		InWavePos--;
	}
}

TapeBit CTapeFeeder::ReadBit()
{
	TapeBit ret;
	ret.Value32 = 0;

	/* fill 'InWaves' */
	while(InWavePos < 4)
	{
		/* get readings */
		while(InWavePos < 4)
			InWaves[InWavePos++] = ReadWave();

		/* do high frequency filter */
/*		while(InWaves[0].Length < ONE_WIDE_MIN)
		{
			InWaves[2].Length += InWaves[0].Length + InWaves[1].Length;
			ShiftWavesLeft(2);
		}*/
	}

	/* check: snapshot chunk? */
	if(InWaves[0].Type == TapeWave::SNAPSHOTCK)
	{
		ret.SNChunk = InWaves[0].SNChunk;
		ret.Type = TapeBit::SNAPSHOTCK;
		return ret;
	}

	/* condition under which we recognise a '0' */

	/*
		498 861 is not a 0
		816 544 is a zero
	*/

	int Length = InWaves[0].Length + InWaves[1].Length;
	if(
		Length >= WAVE_MIN && Length < WAVE_MAX &&
		(
			(
				InWaves[0].Length >= ZERO_THIN_MIN &&
				!(InWaves[1].Length < ZERO_WIDE_MIN || InWaves[1].Length >= ZERO_WIDE_MAX)
			)
			||
			(
				InWaves[1].Length >= ZERO_THIN_MIN &&
				!(InWaves[0].Length < ZERO_WIDE_MIN || InWaves[0].Length >= ZERO_WIDE_MAX)
			)
		)
	)
	{
#ifdef SHOWLOGIC
		printf("0: %d %d\n", InWaves[0].Length, InWaves[1].Length);
#endif
		ret.Length = InWaves[0].Length + InWaves[1].Length;
		ret.Value8 = 0;
		ret.Type = TapeBit::DATA;

		ShiftWavesLeft(2);
		return ret;
	}

	/* condition under which we recognise a '1' */
	bool OneGood = false, OneBad = false;
	int c = 4;
	while(c--)
	{
		if(InWaves[c].Length < ONE_THIN_MAX)
			OneGood = true;
		if(InWaves[c].Length < ONE_WIDE_MIN && InWaves[c].Length >= ONE_WIDE_MAX)
			OneBad = false;
	}

	Length += InWaves[2].Length + InWaves[3].Length;
	if(
		Length >= WAVE_MIN && Length < WAVE_MAX &&
		OneGood && !OneBad
	)
	{
#ifdef SHOWLOGIC
		printf("1: %d %d %d %d\n", InWaves[0].Length, InWaves[1].Length, InWaves[2].Length, InWaves[3].Length);
#endif
		ret.Length = InWaves[0].Length + InWaves[1].Length + InWaves[2].Length + InWaves[3].Length;
		ret.Value8 = 1;
		ret.Type = TapeBit::DATA;

		ShiftWavesLeft(4);
		return ret;
	}

	/* default: gap */
#ifdef SHOWLOGIC
	printf("gap: %d %d %d %d\n", InWaves[0].Length, InWaves[1].Length, InWaves[2].Length, InWaves[3].Length);
#endif
	ret.Length = InWaves[0].Length;
	ret.Type = TapeBit::GAP;
	ShiftWavesLeft(1);

	return ret;
}

/* default WriteBit, which does nothing - e.g. read only media */
bool CTapeFeeder::WriteBit(TapeBit)
{
	return false;
}

Uint32 CTapeFeeder::GetLength()
{
	return 0;
}
