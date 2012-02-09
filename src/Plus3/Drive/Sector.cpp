#include "Drive.h"
#include <malloc.h>

DriveTrack::DriveTrack()
{
	Data8 = NULL;
	Data32 = NULL;
	BitLength = AllocBits = Cursor = 0;

	CRCGenerator.Setup(0x1021, 0xcdb4);
}

/* NB: the following is a count of bits! 262144 = 32kb */
#define ALLOC_WINDOW	262144

void DriveTrack::AddLength(int minlength)
{
	if(Cursor+minlength > AllocBits)
	{
		Data8 = (Uint8 *)realloc(Data8, (AllocBits+ALLOC_WINDOW) >> 3);
		Data32 = (Uint32 *)realloc(Data32, sizeof(Uint32)*((AllocBits+ALLOC_WINDOW) >> 3));
		AllocBits += ALLOC_WINDOW;
	}
}

DriveTrack::~DriveTrack()
{
	if(Data8)
	{
		free(Data8);
		Data8 = NULL;
	}

	if(Data32)
	{
		free(Data32);
		Data32 = NULL;
	}
}

void DriveTrack::ResetCRC()
{
	CRCGenerator.CRCReset();
}

void DriveTrack::WriteCRC()
{
	Uint16 CRC = CRCGenerator.CRCGet();
	WriteValue(CRC&0xff);
	WriteValue(CRC >> 8);
}

void DriveTrack::WriteValue(Uint8 D8, Uint32 D32)
{
//	AddLength(16);
//	Uint16 Encoded8 = CRCGenerator.MFMInflate(D8);
}

void DriveTrack::WriteSpecial(Uint16)
{
}
