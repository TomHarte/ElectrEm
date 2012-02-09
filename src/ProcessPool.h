#ifndef __PROCESSPOOL
#define __PROCESSPOOL

#include "SDL.h"
#include "SDL_thread.h"

class CComponentBase;
class CUEFChunk;

/* some well defined component numbers */
#define COMPONENT_CPU		0
#define COMPONENT_ULA		1
#define COMPONENT_DISPLAY	2
#define COMPONENT_TAPE		3
#define COMPONENT_PLUS1		4
#define COMPONENT_PLUS3		5

enum PPMessage
{
	PPM_CPUDIED, PPM_TAPEDATA_START, PPM_TAPEDATA_STOP, PPM_UNKNOWNOP,
	PPM_QUIT, PPM_HARDRESET, PPM_FSTOGGLE, PPM_ICONIFY, PPM_GUI,
	PPM_TAPEDATA_TRANSIENT
};

class CDisplay;
class C6502;
class CULA;
class CTape;
class CWD1770;
class CPlus1;

/* things of interest to calling threads */

#define ELECTRON_RESET_TIME		(39936*75)

#include "ProcessPoolDebugCodes.h"

#define PPMEDIA_TAPE		0x1
#define PPMEDIA_DISC		0x2

#define ROMTYPE_ALL			0x00
#define ROMTYPE_NOTBASIC	0x01
#define ROMTYPE_DFS			0x02
#define ROMTYPE_ADFS		0x03
#define ROMTYPE_FS			0x04
#define ROMTYPE_LANGUAGE	0x05

#define IOCTLBUFFER_LENGTH	32

#define PPOOLIOCTL_BREAK		0x400
#define PPOOLIOCTL_MUTEXWAIT	0x401

#include "Configuration/ElectronConfiguration.h"

class CProcessPool
{
	public:
		CProcessPool(ElectronConfiguration &);
		~CProcessPool();
		
		CDisplay * Display()
		{
			return Disp;
		}

		/* this lot for a calling process - e.g. a GUI or whatever */

			/* opens a UEF */
			bool Open(char *name);
			void Close(Uint32);

			/* save state and dump memory */
			bool SaveState(char *name);
			bool MemoryDump(char *name);

			/* retrieves machine type */
			bool GetConfiguration(ElectronConfiguration &);

			/* insert a new Component the ProcessPool otherwise knows
			nothing about */
			bool InsertComponent(CComponentBase *);

			/* starts the emulation to run indefinitely. Emulation runs
			in its own thread, but pushes user events to the SDL event
			queue (and blocks pending a new Go) whenever a debugging
			event occurs. Set NumCycles to 0 to run indefinitely, or
			to some positive integer to run for that number of cycles
			only */
			int Go(bool halt = false);

			/* stops emulation, but preserves machine state */
			void Stop();

			/* if a read or write is attempted to a monitor address,
			Go returns */
			void SetDebugAddress(Uint16);
			void ClearDebugAddress(Uint16);
			void ClearAllDebugAddresses();

			/* sets execution limits */
			void SetCycleLimit(Uint32);

			/* takes a combination of PPDEBUG_??? flags in order to
			determine what elmulation events to debug */
			void SetDebugFlags(Uint32);
			void AddDebugFlags(Uint32);
			void RemoveDebugFlags(Uint32);

		/* some of potential interest to both Components & calling
		threads */
			CComponentBase *GetWellDefinedComponent(Uint32 id);
			void SuspendComponent(Uint32 id);
			void ResumeComponent(Uint32 id);

		/* this lot are for connected Components to call */
			bool IOCtl(Uint32 Control, void *Parameter = NULL, Uint32 TimeStamp = 0);
			bool Write(Uint16 Addr, Uint32 TimeStamp, Uint8 Data8, Uint32 Data32);
			bool Read(Uint16 Addr, Uint32 TimeStamp, Uint8 &Data8, Uint32 &Data32);

			void CreateTrapAddressSets(int num);
			void SetTrapAddressSet(int num);

			/* claims only for the current set */
				bool ClaimTrapAddressSet(Uint32 id, Uint16 Value, Uint16 Mask = 0xffff);
			/* claims and releases for all sets */
				bool ClaimTrapAddress(Uint32 id, Uint16 Value, Uint16 Mask = 0xffff);
				bool ReleaseTrapAddress(Uint32 id, Uint16 Value, Uint16 Mask = 0xffff);
			/* this one only for current set */
				bool IsTrapAddress(Uint16 Addr);

			void Message(PPMessage, void *parameter = NULL);
			void DebugMessage(Uint32);

		/* this relates to content information and snapshot chunks */
		bool EffectChunk(CUEFChunk *);

		/* passed on a ROM hint */
		bool ROMHint(bool present, char *name, int type);

	private:
		Uint32 TotalCycles;

		class TrapTable
		{
			public :
				TrapTable();
				~TrapTable();
				void Clear();
				Uint32 TrapAddrFlags[2048];
				Uint32 *TrapAddrDevices[256];
		} *CurrentTrapTable, *AllTrapTables;
		Uint32 NumTrapTables;
		Uint32 DebugAddrFlags[2048];

		struct
		{
			bool Enabled;
			bool HasTrapFlags;
			CComponentBase *Component;
		} ConnectedDevices[16];
		Uint32 NumConnectedDevices;

		/* blah */
		CDisplay *Disp;
		C6502 *CPU;
		CULA *ULA;
		CTape *Tape;
		CWD1770 *Disc;
		CPlus1 *Plus1;
		
		void SendInitialIOCtls();
		bool InTape;
		volatile unsigned int InTapeTransient;

		/* thread related */
		SDL_Thread *UpdateThread;
		static int UpdateHelper(void *);
		int Update();
		Uint32 CyclesToRun;
		
		/* these two act versus the main emulation thread */
		void GetExclusivity();
		void ReleaseExclusivity();
		Uint32 MainThreadID;

		volatile bool Quit;
		
		/* things to debug on */
		Uint32 DebugMask;

		/* configuration options that apply directly to the process pool */
		ElectronConfiguration CurrentConfig, NextConfig;
		bool ConfigDirty;

		void SetNonResetConfiguration(ElectronConfiguration *NewCfg, bool force = false);
		void SetResetConfiguration(void);

		/* IOCtl buffer, for thread communications */
		struct IOCtlRecord
		{
			Uint32 Control;
			void *Parameter;
			Uint32 TimeStamp;
		} IOCtlBuffer[IOCTLBUFFER_LENGTH];
		unsigned int IOCtlBufferRead, IOCtlBufferWrite;
		SDL_mutex *IOCtlMutex, *RunningMutex;
		volatile bool MainThreadRunning;
};

#endif
