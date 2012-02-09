#include "Drive.h"
#include "../../HostMachine/HostMachine.h"
#include <memory.h>
#include <malloc.h>

#define SPIN_TIME 400000

/*
	Assume: double density data rate is 250 kbit/sec, single half
	that. To fit nicely into a 5 RPS encoding, this means 6400
	bytes/track (double), 3200 bytes/track (single)

	Single:

		40 bytes &ff

	Then, per sector:

		6 bytes &00
		IDAM: FE, with clock &c7
		6 byte id field (track, side, sector, length, 16bit CRC)

			= 13 bytes

		11 bytes &ff
		6 bytes &00
		Data address mark
		256 data bytes
		16 bit CRC
		10 bytes &ff

			= 300 bytes

	= 313 bytes/sector
		
	Double:

		60 bytes &4e

	Then, per sector:

		12 bytes &00
		3 bytes &a1
		IDAM: FE
		6 byte id field (track, side, sector, length, 16bit CRC)

			= 22 bytes

		22 bytes &4e
		12 bytes &00
		3 bytes &a1
		IDAM: FB
		256 bytes data
		16bit CRC
		24 bytes &4e

			= 320 bytes

	= 342 bytes/sector

	Spin rate is always 300 RPM
*/

CDriveADF::CDriveADF()
{
	FName = NULL;
}

CDriveADF::~CDriveADF()
{
	if(FName)
	{
		if(Dirty)
		{
			if(gzip)
			{
				gzFile Image = gzopen(FName, "wb9");
				gzwrite(Image, Data, Tracks*SectorLength*Sectors*Sides);
				gzclose(Image);
			}
			else
			{
				FILE *Image = fopen(FName, "wb");
				fwrite(Data, 1, Tracks*SectorLength*Sectors*Sides, Image);
				fclose(Image);
			}
		}

		free(FName);
		FName = NULL;
	}
}

#include <string.h>

bool CDriveADF::Open(char *name)
{
	if(FName) free(FName);
	FName = strdup(name);
	
	/* put empty directory into "drive 2" just in case this is an SSD ... */
	memset(Data, 0, 80*256*16*2);

	/* get data from file */
	gzFile Image;
	Image = gzopen(name, "rb");
	if(!Image) return false;

	TrackOffset = Track = 0;
	Dirty = false;

	DataLen = gzread(Image, Data, 80*256*16*2);
	gzclose(Image);

	/* now determine whether was gzip'd, and readonly state */
	FileSpecs FS = GetHost() -> GetSpecs(FName);
	gzip = !(DataLen == FS.Size);
	ReadOnly = (FS.Stats&FS_READONLY) ? true : false;

	/* This is absolutely the worst file format known to man. Deal with it */

	/* double sided type? */
	char *DBLSided[] = { "dsd", "adl", NULL };
	Sides = GetHost() -> ExtensionIncluded(name, DBLSided) ? 2 : 1;

	/* 16 or 10 sectors / track ? */
	char *DFS[] = {"ssd\a", "dsd\a", "img\a", NULL};
	if(DoubleDensity = !GetHost() -> ExtensionIncluded(name, DFS))
	{
		Sectors = 16;
		BytesPerSpin = 5180;

		/* ADFS sanity check */
		if(DataLen < 6*256)
		{
			gzclose(Image);
			return false;
		}
	}
	else
	{
		Sectors = 10;
		BytesPerSpin = 3170;
	}

	SectorLength = 256;
	SectorLengthExponent = 1; // as 128 << 1 = 256

	/* and determine quantity of stored tracks */
	Tracks = 80; //DataLen / (Sides*Sectors*SectorLength);

	Side = 0;
	CyclesPerRevolution = SPIN_TIME;

	return true;
}

/* interleaved */
#define DataPtr8(sector) &Data[((Track*Sectors) + sector)*Sides*SectorLength + Side*Sectors*SectorLength]

#define DDEN_TRACKHEADER	60
#define DDEN_DATAOFFSET		60
#define DDEN_SECTORLEN		342

#define SDEN_TRACKHEADER	40
#define SDEN_DATAOFFSET		30
#define SDEN_SECTORLEN		298

int CDriveADF::GetTrackLength()
{
	if(DDen != DoubleDensity)
		return SPIN_TIME >> (DDen ? 2 : 3);

	return BytesPerSpin << 4;
}

void CDriveADF::GetEvent(DriveEvent *Ev)
{
	/* check first: are we in the same mode as the data?
	Otherwise return only index holes */

	if(DDen != DoubleDensity)
	{
		Ev->Type = DriveEvent::INDEXHOLE;
		Ev->CyclesToStart = SPIN_TIME - TrackOffset;
		Ev->CycleLength = 0;
		return;
	}

	if(DoubleDensity)
	{
		unsigned int ByteOffset = TrackOffset >> 6;

		/* is the next thing a sector ? */
		if(ByteOffset < DDEN_TRACKHEADER + (DDEN_SECTORLEN*Sectors))
		{
			Ev->Type = DriveEvent::SECTOR;
			Ev->Track = Track;
			Ev->Side = Side;
			Ev->Deleted = false;
			Ev->HeadCRCCorrect = Ev->DataCRCCorrect = true;
			Ev->DataLength = SectorLength;
			Ev->DataLengthExponent = SectorLengthExponent;

			Ev->Sector = ((ByteOffset-DDEN_TRACKHEADER + DDEN_SECTORLEN - 1) / DDEN_SECTORLEN);

			Ev->CyclesToStart = ((DDEN_TRACKHEADER + DDEN_DATAOFFSET + (Ev->Sector*DDEN_SECTORLEN)) << 6) - TrackOffset;
			Ev->CycleLength = SectorLength << 6;
			Ev->Data8 = DataPtr8(Ev->Sector);

			Ev->CyclesPerByte = 64;
		}
		else
		{
			/* if not a sector, then next thing is indexhole */
			Ev->Type = DriveEvent::INDEXHOLE;
			Ev->CyclesToStart = SPIN_TIME - TrackOffset;
			Ev->CycleLength = 0;
		}
	}
	else
	{
		unsigned int ByteOffset = TrackOffset >> 7;

//		printf("ByteOffset: %d\n", ByteOffset);

		/* is the next thing a track ? */
		if(ByteOffset < SDEN_TRACKHEADER + (SDEN_SECTORLEN*Sectors))
		{
			Ev->Type = DriveEvent::SECTOR;
			Ev->Track = Track;
			Ev->Deleted = false;
			Ev->HeadCRCCorrect = Ev->DataCRCCorrect = true;
			Ev->DataLength = SectorLength;

			Ev->Sector = ((ByteOffset-SDEN_TRACKHEADER + SDEN_SECTORLEN - 1) / SDEN_SECTORLEN);
//			printf("Sector %d\n", Ev->Sector);

			Ev->CyclesToStart = ((SDEN_TRACKHEADER + SDEN_DATAOFFSET + (Ev->Sector*SDEN_SECTORLEN)) << 7) - TrackOffset;
			Ev->CycleLength = SectorLength << 7;
			Ev->Data8 = DataPtr8(Ev->Sector);

			Ev->CyclesPerByte = 128;
		}
		else
		{
			/* if not a track, then next thing is indexhole */
			Ev->Type = DriveEvent::INDEXHOLE;
			Ev->CyclesToStart = SPIN_TIME - TrackOffset;
			Ev->CycleLength = 0;
//			printf("Indexhole\n");
		}
//		printf("\n");
	}
}

void CDriveADF::SetEventDirty()
{
	Dirty = true;
}

int CDriveADF::GetLine(DriveLine d)
{
	switch(d)
	{
		default: return CDrive::GetLine(d);
		case DL_WPROTECT:  return ReadOnly ? 1 : 0;
		case DL_DDENQUERY: return DoubleDensity ? 1 : 0;
	}
}
