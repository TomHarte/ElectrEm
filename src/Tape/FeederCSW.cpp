/*

	ElectrEm (c) 2000-6 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	Tape/FeederCSW.cpp
	==================

	CSW file reader. Uses cswdll.dll, provided by Fraser Ross to parse
	CSW files for input. CSW files, like UEF, are 100% accurate reproductions
	of original source cassettes (as far as the machine can tell). They use a
	very simple encoding scheme that focusses on accurately recreating zero
	crossings so that data is loaded correctly. CSW files originate from the
	ZX Spectrum world.

*/
#ifdef WIN32

#include "Internal.h"
#include "../HostMachine/HostMachine.h"

//#define CSWLOG

#ifdef CSWLOG
FILE *CSWLog;
#endif

CTapeFeederCSW::CTapeFeederCSW()
{
#ifdef CSWLOG
	CSWLog = fopen("c:\\CSW.log", "wt");
#endif
	OpenFunc = NULL;

#ifdef WIN32
	/* check executable directory and then just general path */
	char *Name = GetHost() -> ResolveFileName("%EXECPATH%/CSW.dll");
#ifdef CSWLOG
	fprintf(CSWLog, "resolved name: %s\n", Name);
#endif
	
	if(!(CSWLib = LoadLibrary(Name)))
		CSWLib = LoadLibrary("CSW.dll");

	delete[] Name;

	if(CSWLib)
	{
		OpenFunc = (OpenType)GetProcAddress(CSWLib, "Open");
		CloseFunc = (CloseType)GetProcAddress(CSWLib, "Close");
		Finished = (FinishedType)GetProcAddress(CSWLib, "Finished");
		GetNextPulseLength = (GNPLType)GetProcAddress(CSWLib, "GetNextPulseLength");
		Rewind = (FRType)GetProcAddress(CSWLib, "FullRewind");
 		Exception = (EPType)GetProcAddress(CSWLib, "IsExceptionThrown");
 		ExceptionText = (ESType)GetProcAddress(CSWLib, "GetExceptionMessage");
	}

	if(!OpenFunc || !CloseFunc || !Exception || !Finished || !GetNextPulseLength || !Rewind || !ExceptionText)
	{
#ifdef CSWLOG
		fprintf(CSWLog, "unable to find DLL\n");
#endif
		OpenFunc = NULL;
		FreeLibrary(CSWLib);
		CSWLib = NULL;
	}

#endif
}

CTapeFeederCSW::~CTapeFeederCSW()
{
	Close();
#ifdef WIN32
	if(CSWLib) FreeLibrary(CSWLib);
#endif
#ifdef CSWLOG
	fclose(CSWLog);
#endif
}

bool CTapeFeederCSW::Open(char *name)
{
	if(!OpenFunc) return false;

	OpenFunc(name);

#ifdef CSWLOG
	bool err = Exception();
	const char *text = ExceptionText();
	if(err) fprintf(CSWLog, "error: %s\n", text);
#endif

	if(Exception())
	{
		const char *i = ExceptionText();
		return false;
	}

	/* Returns true for positive first pulse in file. */
	typedef bool (__stdcall * GIPType)(); GIPType GetInitialPolarity;
	GetInitialPolarity = (GIPType)GetProcAddress(CSWLib, "GetInitialPolarity");
	if(!GetInitialPolarity) return false;
	PolarityHigh = GetInitialPolarity();

	/* Set sampling rate to 2,000,000 as rest of emulator works to a 2 Mhz bus */
	typedef void (__stdcall * SetSampType)(__int32); SetSampType SetSamplingRate;
	SetSamplingRate = (SetSampType)GetProcAddress(CSWLib, "SetSamplingRate");
	if(!SetSamplingRate) return false;
	SetSamplingRate(2000000);

	FilePosition = 0;

	return true;
}

void CTapeFeederCSW::Close()
{
	if(OpenFunc)
		CloseFunc();
}

TapeWave CTapeFeederCSW::ReadWave()
{
	TapeWave wav;
	wav.Type = PolarityHigh ? TapeWave::HIGH : TapeWave::LOW;
	PolarityHigh = !PolarityHigh;

	wav.Length = GetNextPulseLength();

#ifdef CSWLOG
	fprintf(CSWLog, "%u, ", WLength);
#endif

	FilePosition++;
	if(Finished())
	{
		OverRanFlag = true;
		FilePosition = 0;
		Rewind();
	}

	return wav;
}

Uint64 CTapeFeederCSW::Tell()
{
	return FilePosition;
}

void CTapeFeederCSW::Seek(Uint64 pos)
{
#ifdef CSWLOG
	fprintf(CSWLog, "\nfull rewind\n", pos); fflush(CSWLog);
#endif

	Rewind();
	FilePosition = pos;
	while(pos--)
		GetNextPulseLength();
	bool err = Exception();
	const char *text = ExceptionText();
}

bool CTapeFeederCSW::OverRan()
{
	return OverRanFlag;
}

void CTapeFeederCSW::ResetOverRan()
{
	OverRanFlag = false;
}

#endif
