#ifndef __TAPE_H
#define __TAPE_H

#include "../ComponentBase.h"
#include "../UEF.h"
#include "Internal.h"
#include "../Configuration/ElectronConfiguration.h"

#define BIT_LENGTH			1628	/* FOR OUTPUT ONLY!!! - verified by Fraser Ross */

#define TAPEIOCTL_REWIND	0x500

enum TapeModes
{
	TM_INPUT, TM_OUTPUT, TM_OFF
};

class CTape : public CComponentBase
{
	public:
		void AttachTo(CProcessPool &, Uint32 id);
		bool Write(Uint16 Addr, Uint32 TimeStamp, Uint8 Data8, Uint32 Data32);
		bool Read(Uint16 Addr, Uint32 TimeStamp, Uint8 &Data8, Uint32 &Data32);
		Uint32 Update(Uint32 TotalCycles, bool Catchup);
		bool IOCtl(Uint32 Control, void *Parameter = NULL, Uint32 TimeStamp = 0);

		bool SetMode(Uint32 TimeStamp, TapeModes Mode, bool TapeMotor);
		bool Open(char *name);
		void Close();

		virtual ~CTape();
		CTape(ElectronConfiguration &cfg);

		/* this returns the string a user should enter to run the game - usually *RUN"" or CHAIN"" */
		const char *QueryTapeCommand();

	private :
		void RunTo(Uint32 Time);
		Uint32 RunTime;

		bool TapeMotor;
		TapeModes CurrentMode;
		Uint32 TotalTime;

		/* data register */
		Uint64 ScrollRegister32;
		Uint32 ScrollRegister8;
		bool Silence;
		int BitCount, OutputCounter, OutBitCount;

		/* data feeder and related */
		class CTapeFeeder *Feeder;
		void AdvanceBit();
		TapeBit CurrentBit;
		Uint64 StartPos;

		/* fast tape helpers */
		struct
		{
			char Name[11];
			Uint32 LoadAddress, ExecAddress;
			Uint16 BlockNo, BlockLength;
			Uint8 Flags;
			Uint8 Data8[256];
			Uint32 Data32[256];

			int DataPtr;
		} TapeBlock;
		struct FileStats
		{
			char Name[11];
			Uint32 LoadAddr, ExecAddr;
			Uint32 StartAddrOrLength, EndAddrOrFlags;
		};
		void SetTrapAddresses(bool Enabled, bool Slogger64);

		bool GetBlock(bool data = true);
		void TapeGetC(Uint8 &Data8, Uint32 &Data32);
		void DetermineType();
		enum
		{
			LT_RUN, LT_CHAIN, LT_UNKNOWN
		} LoadType;
		bool UseFastHack, TapeHasROMData;
};

#endif
