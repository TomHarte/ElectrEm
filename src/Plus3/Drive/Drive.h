#ifndef __DRIVE_H
#define __DRIVE_H

#include "SDL.h"
#include "../Helper/Helper.h"
#include "fdi2raw/fdi2raw.h"

struct DriveEvent
{
	enum { INDEXHOLE, SECTOR } Type;
	bool HeadCRCCorrect, DataCRCCorrect, Deleted;
	Uint8 Track, Sector, Side;

	Uint8 *Data8;
	Uint32 *Data32;
	Uint32 DataLength, DataLengthExponent;

	Uint32 CyclesToStart, CycleLength, CyclesPerByte;
};

class DriveTrack
{
	public:
		DriveTrack();
		~DriveTrack();

		/* misc control */
		void ResetCRC();

		/* things you might like to write to a track */
		void WriteCRC();
		void WriteValue(Uint8, Uint32 = 0);
		void WriteSpecial(Uint16);

		/* and the things you might like to read */
		Uint8 *Data8;
		Uint32 *Data32;
		int BitLength;
	private:
		int AllocBits, Cursor;
		void AddLength(int);
		CDiscHelper CRCGenerator;
};

enum DriveLine{
	/* real drive lines */
	DL_DIRECTION, DL_STEP, DL_TRACK0, DL_SIDE, DL_DDEN, DL_WPROTECT,

	/* fictional ones */
	DL_INDEXHOLECOUNT, DL_INDEXHOLEFLAG, DL_DDENQUERY
};

class CDrive
{
	public:
		virtual ~CDrive();

		/* general interface */
		virtual void SetLine(DriveLine, int param);
		virtual int GetLine(DriveLine);
		virtual bool Open(char *name);

		/* Update spins the motor for the requested number of cycles, updating the index hole count and flag */
		virtual void Update(Uint32 Cycles);

		/* Sector & index hole level read/write interface - for type II commands */
		virtual void GetEvent(DriveEvent *) = 0;
		virtual void SetEventDirty();
		virtual void PutEvent(DriveEvent *);

		/* Bit level interface - for type III commands */
		virtual void GetTrack(DriveTrack *);
		virtual void PutTrack(DriveTrack *);
		virtual int GetTrackLength() = 0;

	protected:
		bool IndexHoleFlag, DDen;
		int IndexHoleCount;
		Uint32 TrackOffset, CyclesPerRevolution;
		int Track, TrackDir, Side, Tracks;
};

class CDriveEmpty : public CDrive
{
	public:
		CDriveEmpty();
		void GetEvent(DriveEvent *);
		int GetTrackLength();
};

#include "zlib.h"

class CDriveADF : public CDrive
{
	public:
		CDriveADF();
		~CDriveADF();

		/* general interface */
		bool Open(char *name);
		int GetLine(DriveLine);

		/* type II interface */
		void GetEvent(DriveEvent *);
		void SetEventDirty();

		/* type III interface, as far as ADF cares */
		int GetTrackLength();

	private:
		char *FName;
		bool gzip;
		Uint8 Data[80*256*16*2];
		
		unsigned int Sides, Sectors, DataLen, SectorLength, SectorLengthExponent;
		bool Dirty, ReadOnly, DoubleDensity;

		Uint32 BytesPerSpin;
};

#include "fdi2raw/fdi2raw.h"

class CDriveFDI : public CDrive
{
	public:
		CDriveFDI();
		~CDriveFDI();

		/* general interface */
		bool Open(char *name);
		int GetLine(DriveLine);

		/* type II interface */
		void GetEvent(DriveEvent *);

		/* type III interface, as far as ADF cares */
		int GetTrackLength();

	private:
		gzFile File;
		FDI *FDIPtr;
};

#endif
