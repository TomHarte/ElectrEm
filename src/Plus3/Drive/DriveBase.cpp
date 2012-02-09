#include "Drive.h"

/* Base Class */
CDrive::~CDrive() {}

void CDrive::SetLine(DriveLine l, int param)
{
	switch(l)
	{
		default: break;
		case DL_DIRECTION: TrackDir = param; break;
		case DL_STEP:
			Track += TrackDir;
			if(Track < 0) Track = 0;
			if(Track >= Tracks) Track = Tracks-1;
		break;
		case DL_SIDE: Side = param; break;
		case DL_INDEXHOLECOUNT:	IndexHoleCount = param; break;
		case DL_INDEXHOLEFLAG:	IndexHoleFlag = param ? true :  false; break;
		case DL_DDEN: DDen = param ? true : false; break;
	}
}

int CDrive::GetLine(DriveLine l)
{
	switch(l)
	{
		case DL_TRACK0: return !Track;
		case DL_DIRECTION: return TrackDir;
		case DL_INDEXHOLECOUNT: return IndexHoleCount;
		case DL_INDEXHOLEFLAG: return IndexHoleFlag ? 1 : 0;
		default: return 0;
	}
}

void CDrive::Update(Uint32 Cycles)
{
	/* NB: 300 RPM => 5 RPS => 1 spin every 400,000 cycles */
	Cycles += TrackOffset;
	int NewOffset = Cycles%CyclesPerRevolution;
	int Spins = (Cycles-NewOffset) / CyclesPerRevolution;

	IndexHoleCount += Spins;
	if(Spins) IndexHoleFlag = true;

	TrackOffset = NewOffset;
}


void CDrive::PutEvent(DriveEvent *) {}
bool CDrive::Open(char *) { return false; }

#define DDEN_IDAM		0x4489
#define SDEN_HEADERMARK	0xf57e
#define SDEN_DATAMARK	0xf56f

void CDrive::GetTrack(DriveTrack *d)
{
	/* use sector interface to create track */
	if(DDen)
	{
		/* write index pulse -> first sector information */
		unsigned int c = 60;
		while(c--)
			d->WriteValue(0x4e);

		/* find index hole */
		DriveEvent NewSector;
		do
			GetEvent(&NewSector);
		while(NewSector.Type != DriveEvent::INDEXHOLE);

		/* now write sectors until index hole */
		while(1)
		{
			GetEvent(&NewSector);
			if(NewSector.Type != DriveEvent::SECTOR)
				break;

			c = 12;
			while(c--)
				d->WriteValue(0);

			c = 3;
			while(c--)
				d->WriteSpecial(DDEN_IDAM);

			d->ResetCRC();
			d->WriteSpecial(0xfe);
			d->WriteValue(NewSector.Track);
			d->WriteValue(NewSector.Side);
			d->WriteValue(NewSector.Sector);
			d->WriteValue((Uint8)NewSector.DataLengthExponent);
			d->WriteCRC();

			c = 22;
			while(c--)
				d->WriteValue(0x4e);

			c = 12;
			while(c--)
				d->WriteValue(0);

			c = 3;
			while(c--)
				d->WriteSpecial(DDEN_IDAM);


			d->ResetCRC();
			d->WriteSpecial(0xfb);
			for(c = 0; c < NewSector.DataLength; c++)
				d->WriteValue(NewSector.Data8[c], NewSector.Data32[c]);
			d->WriteCRC();

			c = 24;
			while(c--)
				d->WriteValue(0x4e);
		}

		/* fill in to index hole... */
		int ByteCount = (GetTrackLength() - d->BitLength) >> 4;
		while(ByteCount--)
			d->WriteValue(0x4e);
	}
	else
	{
		/* write index pulse -> first sector information */
		unsigned int c = 40;
		while(c--)
			d->WriteValue(0xff);

		/* find index hole */
		DriveEvent NewSector;
		do
			GetEvent(&NewSector);
		while(NewSector.Type != DriveEvent::INDEXHOLE);

		/* now write sectors until index hole */
		while(1)
		{
			GetEvent(&NewSector);
			if(NewSector.Type != DriveEvent::SECTOR)
				break;

			c = 6;
			while(c--)
				d->WriteValue(0);

			d->ResetCRC();
			d->WriteSpecial(SDEN_HEADERMARK);
			d->WriteValue(NewSector.Track);
			d->WriteValue(NewSector.Side);
			d->WriteValue(NewSector.Sector);
			d->WriteValue((Uint8)NewSector.DataLengthExponent);
			d->WriteCRC();

			c = 11;
			while(c--)
				d->WriteValue(0xff);

			c = 6;
			while(c--)
				d->WriteValue(0);

			d->ResetCRC();
			d->WriteSpecial(SDEN_DATAMARK);

			for(c = 0; c < NewSector.DataLength; c++)
				d->WriteValue(NewSector.Data8[c], NewSector.Data32[c]);
			d->WriteCRC();

			c = 10;
			while(c--)
				d->WriteValue(0xff);
		}

		/* fill in to index hole... */
		int ByteCount = (GetTrackLength() - d->BitLength) >> 4;
		while(ByteCount--)
			d->WriteValue(0xff);
	}
}

void CDrive::PutTrack(DriveTrack *)
{
	/* break into sectors and pass to sector interface */
}

/* Empty Drive implementation */
#define SPIN_TIME 400000

CDriveEmpty::CDriveEmpty()
{
	CyclesPerRevolution = SPIN_TIME;
	Tracks = 80;
	Track = 0;
	TrackOffset = 0;
}

void CDriveEmpty::GetEvent(DriveEvent *Ev)
{
	/* nothing but index holes */
	Ev->Type = DriveEvent::INDEXHOLE;
	Ev->CyclesToStart = SPIN_TIME - TrackOffset;
	Ev->CycleLength = 0;
}

int CDriveEmpty::GetTrackLength()
{
	return SPIN_TIME >> (DDen ? 2 : 3);
}
void CDrive::SetEventDirty() {}
