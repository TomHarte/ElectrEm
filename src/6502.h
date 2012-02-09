#ifndef __6502_H
#define __6502_H

#include "ProcessPool.h"
#include "ComponentBase.h"
#include "SDL.h"
#include "SDL_thread.h"

#define OneMhz_BUS	0
#define TwoMhz_BUS	1
#define RAM			2

#define BUS_MASK	127
#define BUS_SHIFT	7
//#define BUS_REPEAT	40064

union BrokenWord
{
	Uint16 a;

	struct
	{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		Uint8 l, h;
#else
		Uint8 h, l;
#endif
	}b;
};

struct C6502State
{
	Uint8 a8, x8, y8, s, p8;
	Uint32 a32, x32, y32, p32;
	BrokenWord pc;
};

#define C6502IOCTL_FINISHOPCODE		0x200

/*

	These are for the use of whatever provides the gather addresses.

*/
#define Transpose6502Address8(addr)		(addr&~0xff) + (addr >> 2)
#define Transpose6502Address32(addr)	addr + ((addr&~0xff) >> 2) + 64

class C6502 : public CComponentBase
{
	public:
		C6502();
		virtual ~C6502();

		Uint32 Update(Uint32, bool);
		void AttachTo(CProcessPool &pool, Uint32 id);

		/* for control purposes */
		void EstablishMemoryLayouts(int);
		void SetMemoryLayout(int);
			void SetReadPage(int LocalAddr, int GlobalAddr, int Length);
			void SetWritePage(int LocalAddr, int GlobalAddr, int Length);
			void SetRepeatedWritePage(int LocalAddr, int GlobalAddr, int Length);
			void SetExecCyclePage(int LocalAddr, Uint32 **Table, int Length);

			/* this one adjusts wide paging only */
			void SetReadPage32(int LocalAddr, int GlobalAddr, int Length);

		void SetGathering(Uint16 *AddressSource, Uint32 *Target32);
		void SetMemoryView(int pos, int layout);

		Uint32 GetCyclesExecuted();
		bool IOCtl(Uint32 Control, void *Parameter = NULL, Uint32 TimeStamp = 0);

		void SetInstructionLimit(Uint32);

		/* memory allocation and access */
		bool SetMemoryTotal(int);
		int GetStorage(int);
		void WriteMemoryBlock(int Base, int Offset, int Length, Uint8 *Data8, Uint32 *Data32 = NULL);
		void ReadMemoryBlock(int Base, int Offset, int Length, Uint8 *Data8, Uint32 *Data32 = NULL);

		/* for state snapshot & tape hack purposes */
		void SetState(C6502State &);
		void GetState(C6502State &);

		void WriteMem(Uint16 OpAddr, Uint16 WriteAddr, Uint8 Data8, Uint32 Data32);
		void WriteMem(Uint16 OpAddr, Uint16 WriteAddr, Uint8 Data8);
		void ReadMem(Uint16 OpAddr, Uint16 ReadAddr, Uint8 &Data8, Uint32 &Data32);
		void ReadMem(Uint16 OpAddr, Uint16 ReadAddr, Uint8 &Data8);

		void Pop();
		void Push(Uint8, Uint32 = 0);

	private :
		static int CPUThreadHelper(void *tptr);
		int CPUThread();
		volatile bool CPUDead;

		/* Register set */
		Uint8 a8, x8, y8, s;
		Uint32 a32, x32, y32;

		struct
		{
			Uint32 Carry32;
			Uint8 Carry, Neg, Overflow, Zero, Misc;
		} Flags;
		BrokenWord pc;

		/* Memory management */
		struct MemoryLayout
		{
			Uint8 *Read8Ptrs[256], *Write8Ptrs[256];
			Uint32 *Read32Ptrs[256], *Write32Ptrs[256];
			Uint32 **ExecCyclePtrs[256];
		};

		Uint32 *TrapFlags;

		MemoryLayout *CurrentView[8];
		MemoryLayout *AllLayouts, *CurrentLayout;
		int NumLayouts;

		bool ZeroPageClear, StackPageClear;

		/* Timing & Gathering */
		Uint32 *GatherTarget, *GatherTargetStart;
		Uint16 *GatherAddresses, *GatherAddressesStart;

		/* Allocated memory */
		Uint32 *MemoryPool;
		int AllocatedBytes;
		int AllocTarget;

		/* CPU thread & synchronisation */
		SDL_sem *GoSemaphore, *StopSemaphore;
		SDL_Thread *MainEx;
		volatile bool Quit;

		/*

			cycle counting mechanisms - thread shared, so 'volatile'
			In reality, these are cast back and forth from volatile
			as and when necessary, in an optimal fashion

			Compiler could not be expected to spot this optimisation,
			as it depends on knowing in that volatility is necessary
			only before and after interaction with semaphores, etc

		*/
		Uint32 CyclesToRun, TotalCycleCount, SubCycleCount, FrameCount;
		Uint32 InstructionsToRun;

		volatile Uint32 VolCyclesToRun, VolTotalCycleCount, VolSubCycleCount, VolFrameCount;
		volatile Uint32 VolInstructionsToRun;

		volatile bool IRQLine, RSTLine, NMILine, ForceRST;

		/* a rare conversion from macro to function */
		void Break();
		void ILoopRun();
		inline bool CycleDoneT(int CycleDownCount, MemoryLayout *CMem, bool &QuitEarly);
		inline bool CycleDoneNotEarlyT(int CycleDownCount, MemoryLayout *CMem);
};

#endif
