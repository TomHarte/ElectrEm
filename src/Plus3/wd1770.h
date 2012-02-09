#ifndef __WD1770_H
#define __WD1770_H

#include "../ComponentBase.h"
#include "../Configuration/ElectronConfiguration.h"
#include "SDL.h"
#include "SDL_thread.h"
#include "Drive/Drive.h"

#define WDOPEN_FAIL	0
#define WDOPEN_SDEN	1
#define WDOPEN_DDEN	2

class CWD1770 : public CComponentBase
{
	public:
		CWD1770(ElectronConfiguration &cfg);
		virtual ~CWD1770();
		bool IOCtl(Uint32 Control, void *Parameter = NULL, Uint32 TimeStamp = 0);

		bool Write(Uint16 Addr, Uint32 TimeStamp, Uint8 Data8, Uint32 Data32);
		bool Read(Uint16 Addr, Uint32 TimeStamp, Uint8 &Data8, Uint32 &Data32);
		Uint32 Update(Uint32 TotalCycles, bool Catchup);
		void AttachTo(CProcessPool &pool, Uint32 id);

		int Open(char *name, int drive = 0); /* returns one of the WDOPEN_????s, depending on the native density of track 0 of the media opened */
		void Close(int drive = 0);

	private:
		static int MainThreadHelper(void *tptr);
		int MainThread();
		void UpdateTo(Uint32 CycleTime);

		volatile Uint32 CyclesToRun;
		volatile Uint32 IdleTime;
		Uint32 LastRun, CurrentTime;
		SDL_sem *GoSemaphore, *StopSemaphore;
		SDL_Thread *MainEx;
		volatile bool Quit;

		/* registers, etc - shared between threads, so... */
		volatile Uint8 DataShift8, Data8;
		volatile Uint32 DataShift32, Data32;
		volatile Uint8 Track, Sector, Command, Status, Control;
		volatile bool NewCmmd;
		volatile bool ForceInterrupt, IndexHoleInterrupt;
		void DoCommand();

		/* drive motors */
		int MotorOn;

		/* connected drives */
		volatile struct
		{
			CDrive *Drive;
			bool ReadOnly;
		} Drives[2], *CurrentDrive;
		bool DDen;

		/* type II helpers */
		void GetSector();
		DriveEvent CurSector;
};

#endif
