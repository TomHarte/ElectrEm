#ifndef __INTERNAL_H
#define __INTERNAL_H

#include "../UEF.h"

enum EventType {
	TE_WAVE, TE_PULSE, TE_BIT, TE_GAP,

	TE_SNAPSHOT, TE_END
};

struct TapeEvent
{
	EventType Type;
	Uint16 Phase;
	Uint32 Length;
	float BaudRate;

	union
	{
		struct
		{
			bool High;
		} Pulse;

		struct
		{
			Uint8 Data8;
			Uint32 Data32;
		} Bit;

		struct
		{
			CUEFChunk *SNChunk;
		} Snapshot;

		struct
		{
			float Rate;
		} Baud;

		struct
		{
			Uint16 Angle;
		} Phase;
	} Data;
};

enum TapeEventConversions
{
	CONV_PULSE, CONV_BIT, CONV_NONE
};

#define TEQ_READONLY	0x01

class TapeEventQueue
{
	public:
		TapeEventQueue();
		virtual ~TapeEventQueue();

		/*
			GetEvent copies the event at the cursor to the passed
			structure, and advances the cursor

			It will do (bit/pulse/wave) -> (bit/pulse/wave)
			conversions if requested
		*/
		void GetEvent(TapeEvent *, TapeEventConversions Converter = CONV_NONE);

		/*
			PutEvent adds an event at the cursor, either pushing
			all other events along or overwriting them (latter is
			default)
		*/
		void PutEvent(TapeEvent &, bool Overwrite = true);

		/*
			Seek moves the cursor, taking an argument in cycles -
			negative or positive
		*/
		void Seek(Sint32 Cycles);

		/*
			Open & Close
		*/
		virtual bool Open(char *name) = 0;
		virtual void Close();

		/*
			Some flags & stats
		*/
		virtual Uint32 GetFlags() = 0;
		virtual Uint64 GetLength() = 0;
		virtual char *GetTitle();
		virtual char *GetCreator();

		/*
			NB: for inherited use, not for general access
		*/
		void SeedEventList(Uint64);
		TapeEvent *GetNewEvent();

	protected:
		/*

			GenerateEvents is the one inheriting classes should
			provide - parameter is the requested start time
		*/
		virtual void GenerateEvents(Uint64) = 0;
		/*
			Flush is also for inheriting classes - it is called
			whenever changes have occured to the current list
			since it was generated. Default function does nothing
			(i.e. tape image is read only)

			Parameter is start time (again)
		*/
		virtual void Flush(Uint64);


		struct TEHolder
		{
			TEHolder *Next;
			TapeEvent Event;
		} *Head, **ListPtr, **EndPtr, *Diversion, *DeadList, **DeadPtr;
		void FreeList(TEHolder *);
		bool Dirty;
		Uint64 Start, IntendedStart, End, CursorTime;
};

class TapeEventQueueUEF: public TapeEventQueue
{
	public:
		bool Open(char *name);
		void Close();
		Uint32 GetFlags();
		Uint64 GetLength();
		char *GetTitle();
		char *GetCreator();
		virtual ~TapeEventQueueUEF();
		TapeEventQueueUEF();

	private:
		void GenerateEvents(Uint64);
		void Flush(Uint64);

		void GetNewFeeder();
		class CUEFTapeChunk *CSource;

		Uint32 LastChunk;
		Uint64 LastWantTime, LastFoundTime;

		CUEFChunkSelector *TSelector;

		float BaudRate;
		Uint16 Phase;
};

struct TapeWave
{
	enum
	{
		HIGH, LOW, GAP, SNAPSHOTCK
	} Type;

	Uint16 Phase;
	Uint32 Length;
	CUEFChunk *SNChunk;
	TapeWave() {Phase = 0; Length = 0; SNChunk = NULL; Type = GAP;}
};

struct TapeBit
{
	Uint32 Length;
	Uint8 Value8;
	Uint32 Value32;
	CUEFChunk *SNChunk;
	enum
	{
		GAP, DATA, SNAPSHOTCK
	} Type;
	TapeBit() {Length = Value32 = 0; Value8 = 0; SNChunk = NULL; Type = GAP;}
};

class CTapeFeeder
{
	public:
		CTapeFeeder();
		virtual ~CTapeFeeder();

		/* open and close functions */
		virtual bool Open(char *name) = 0;
		virtual void Close() = 0;

		/* wave reading function */
		virtual TapeWave ReadWave() = 0;

		/* bit reading function */
		virtual TapeBit ReadBit();

		/* bit writing function (no need for wave writing) */
		virtual bool WriteBit(TapeBit);

		/* positioning functions. Tell returns some number that indicates the current location, and Seek returns to a value returned by Tell */
		virtual Uint64 Tell() = 0;
		virtual void Seek(Uint64) = 0;
		virtual Uint32 GetLength();

		/* for testing whether tape has looped */
		virtual bool OverRan() = 0;
		virtual void ResetOverRan() = 0;

	private:
		TapeWave InWaves[4];
		int InWavePos;
		void ShiftWavesLeft(int positions);
};

class CTapeFeederUEF: public CTapeFeeder
{
	public:
		CTapeFeederUEF();
		virtual ~CTapeFeederUEF();

		bool Open(char *name);
		void Close();

		TapeWave ReadWave();
		TapeBit ReadBit();

		Uint64 Tell();
		void Seek(Uint64);

		bool OverRan();
		void ResetOverRan();
		Uint32 GetLength();

	private:
		CUEFChunkSelector *TSelector;
		void GetNewSource();
		Uint32 Waves;

		class CUEFChunkFeeder *CSource;
		float BaudRate;
		Uint16 Phase;
};

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class CTapeFeederCSW: public CTapeFeeder
{
	public:
		CTapeFeederCSW();
		~CTapeFeederCSW();

		bool Open(char *name);
		void Close();

		TapeWave ReadWave();

		Uint64 Tell();
		void Seek(Uint64);

		bool OverRan();
		void ResetOverRan();
	private:
		bool OverRanFlag;
		bool PolarityHigh;

		Uint64 FilePosition;

#ifdef WIN32
		HINSTANCE CSWLib;
#endif
		/* Parameter is the file path and name null terminated. */
		typedef void (__stdcall * OpenType)(const char *); OpenType OpenFunc;
		typedef void (__stdcall * CloseType)(); CloseType CloseFunc;

		/* return true for eof */
		typedef bool (__stdcall * FinishedType)(); FinishedType Finished;

		/* Returns a pulselength */
		typedef unsigned int (__stdcall * GNPLType)(); GNPLType GetNextPulseLength;

		/* Resets internal state and effectively rewinds tape. Does not close file. */
		typedef void (__stdcall * FRType)(); FRType Rewind;

		/* check for errors */
		typedef bool (__stdcall * EPType)(); EPType Exception;
		typedef const char * (__stdcall *ESType)(); ESType ExceptionText;
};
#endif

#endif
