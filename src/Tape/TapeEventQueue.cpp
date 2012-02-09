#include "Internal.h"

#define TIME_WINDOW (1 << 23)

TapeEventQueue::TapeEventQueue()
{
	CursorTime = Start = End = 0;
	Dirty = false;
	Head = NULL; ListPtr = EndPtr = &Head;
	DeadList = NULL; DeadPtr = &DeadList;

	/* seed a four member linked list for Diversion */
	Diversion = new TEHolder;
	Diversion->Next = new TEHolder;
	Diversion->Next->Next = new TEHolder;
	Diversion->Next->Next->Next = new TEHolder;
}

TapeEventQueue::~TapeEventQueue()
{
	if(Dirty) Flush(Start);
	FreeList(Head);
	FreeList(DeadList);

	Diversion->Next->Next->Next->Next = NULL;
	FreeList(Diversion);
	Close();
}

void TapeEventQueue::FreeList(TEHolder *SPtr)
{
	if(SPtr != Head)
	{
		/* free for real! */
		TEHolder *EvPtr = SPtr, *Next;
		while(EvPtr)
		{
			Next = EvPtr->Next;
			delete EvPtr;
			EvPtr = Next;
		}
	}
	else
	{
		/* add to dead list */
		if(SPtr)
		{
			*DeadPtr = SPtr;
			DeadPtr = &(*EndPtr)->Next;
		}

		/* check if this was main list */
		Head = NULL;
		ListPtr = EndPtr = &Head;
	}
}

void TapeEventQueue::Seek(Sint32 Cycles)
{
	CursorTime += Cycles;

	/* first thing: do we need to fix up the cache? */
	if(CursorTime < Start || CursorTime > End || Start == End)
	{
		if(Dirty) Flush(Start);
		GenerateEvents(IntendedStart = (CursorTime & ~(TIME_WINDOW-1)));
	}

	/* push CursorTime backwards to an event boundary */
	Uint64 CTime = Start;
	ListPtr = &Head;
	while(*ListPtr && (CTime+(*ListPtr)->Event.Length) <= CursorTime)
	{
		CTime += (*ListPtr)->Event.Length;
		ListPtr = &(*ListPtr)->Next;
	}
	CursorTime = CTime;
}

void TapeEventQueue::SeedEventList(Uint64 StartTime)
{
	FreeList(Head);
	Start = End = StartTime;
}

TapeEvent *TapeEventQueue::GetNewEvent()
{
	if(End > (IntendedStart+TIME_WINDOW))
		return NULL;

	if(*EndPtr)
	{
		End += (*EndPtr)->Event.Length;
		EndPtr = &(*EndPtr)->Next;
	}

	if(DeadList)
	{
		*EndPtr = DeadList;

		DeadList = DeadList->Next;
		if(*DeadPtr == DeadList)
			DeadPtr = &DeadList;
	}
	else
		*EndPtr = new TEHolder;

	(*EndPtr)->Next = NULL;

	return &(*EndPtr)->Event;
}

void TapeEventQueue::Close() {}
void TapeEventQueue::Flush(Uint64) {}

/* 1200 baud +- 40%, in 2Mhz cycles dumkopf */

#define ONE_WIDE_MIN	250
#define ONE				417
#define ONE_THIN_MAX	550
#define ONE_WIDE_MAX	600

#define ZERO_WIDE_MIN	490
#define ZERO_THIN_MIN	680
#define ZERO			833
#define ZERO_WIDE_MAX	1100

#define WAVE_MIN	1250
#define WAVE_MAX	2100

void TapeEventQueue::GetEvent(TapeEvent *Target, TapeEventConversions Converter)
{
	if(Start == End)
		Seek(0);

	if(!(*ListPtr))
	{
		Target->Type = TE_END;
	}
	else
	{
		switch((*ListPtr)->Event.Type)
		{
			default:
				*Target = (*ListPtr)->Event;
				ListPtr = &(*ListPtr)->Next;
			break;

			case TE_WAVE:
			case TE_PULSE:
			case TE_BIT:
				switch(Converter)
				{
					case CONV_NONE:
						*Target = (*ListPtr)->Event;
						ListPtr = &(*ListPtr)->Next;
					break;

					case CONV_PULSE:
						if(((*ListPtr)->Event.Type == TE_BIT) && (*ListPtr)->Event.Data.Bit.Data8)
						{
							/* low/high/low/high */
							TEHolder *Ptr = Diversion;
							Ptr->Event.Type = TE_PULSE;
							Ptr->Event.Data.Pulse.High = false;
							Ptr->Event.Length = ((*ListPtr)->Event.Length+2) >> 2;
							Ptr->Event.Phase = (*ListPtr)->Event.Phase;
							Ptr->Event.BaudRate = (*ListPtr)->Event.BaudRate;

							Ptr = Ptr->Next;
							Ptr->Event.Type = TE_PULSE;
							Ptr->Event.Data.Pulse.High = true;
							Ptr->Event.Length = ((*ListPtr)->Event.Length >> 2) + ((*ListPtr)->Event.Length&1);
							Ptr->Event.Phase = (*ListPtr)->Event.Phase;
							Ptr->Event.BaudRate = (*ListPtr)->Event.BaudRate;

							Ptr = Ptr->Next;
							Ptr->Event.Type = TE_PULSE;
							Ptr->Event.Data.Pulse.High = false;
							Ptr->Event.Length = ((*ListPtr)->Event.Length+2) >> 2;
							Ptr->Event.Phase = (*ListPtr)->Event.Phase;
							Ptr->Event.BaudRate = (*ListPtr)->Event.BaudRate;

							Ptr = Ptr->Next;
							Ptr->Event.Type = TE_PULSE;
							Ptr->Event.Data.Pulse.High = true;
							Ptr->Event.Length = (*ListPtr)->Event.Length >> 2;
							Ptr->Event.Phase = (*ListPtr)->Event.Phase;
							Ptr->Event.BaudRate = (*ListPtr)->Event.BaudRate;

							Ptr->Next = (*ListPtr)->Next;
							ListPtr = &Diversion;
						}

						if(
							((*ListPtr)->Event.Type == TE_WAVE) ||
							(((*ListPtr)->Event.Type == TE_BIT) && !(*ListPtr)->Event.Data.Bit.Data8)
							)
						{
							/* convert a wave into pulses */
							TEHolder *Ptr = Diversion->Next->Next;
							Ptr->Event.Type = TE_PULSE;
							Ptr->Event.Data.Pulse.High = false;
							Ptr->Event.Length = ((*ListPtr)->Event.Length+1) >> 1;
							Ptr->Event.Phase = (*ListPtr)->Event.Phase;
							Ptr->Event.BaudRate = (*ListPtr)->Event.BaudRate;

							Ptr = Ptr->Next;
							Ptr->Event.Type = TE_PULSE;
							Ptr->Event.Data.Pulse.High = true;
							Ptr->Event.Length = (*ListPtr)->Event.Length >> 1;
							Ptr->Event.Phase = (*ListPtr)->Event.Phase;
							Ptr->Event.BaudRate = (*ListPtr)->Event.BaudRate;

							Ptr->Next = (*ListPtr)->Next;
							ListPtr = &Diversion->Next->Next;
						}

						*Target = (*ListPtr)->Event;
						ListPtr = &(*ListPtr)->Next;
					break;

					case CONV_BIT:{
						/* no problem if already a bit */
						if(
							(*ListPtr)->Event.Type == TE_BIT
						)
						{
							*Target = (*ListPtr)->Event;
							ListPtr = &(*ListPtr)->Next;
						}

						/* a wave of the right sort of length is a 0 bit, straight off */
						if(
							(*ListPtr)->Event.Type == TE_WAVE &&
							(*ListPtr)->Event.Length >= WAVE_MIN &&
							(*ListPtr)->Event.Length < WAVE_MAX
							)
						{
							*Target = (*ListPtr)->Event;
							Target->Type = TE_BIT;
							Target->Data.Bit.Data8 = 0;
							Target->Data.Bit.Data32 = 0;
							ListPtr = &(*ListPtr)->Next;

							break;
						}

						/* okay, forget it, break down to pulses */
						TapeEvent Pulses[4];

						GetEvent(&Pulses[0], CONV_PULSE);
						GetEvent(&Pulses[1], CONV_PULSE);

						int Length = Pulses[0].Length + Pulses[1].Length;
						if(
							Length >= WAVE_MIN && Length < WAVE_MAX &&
							(
								(
									Pulses[0].Length >= ZERO_THIN_MIN &&
									!(Pulses[1].Length < ZERO_WIDE_MIN || Pulses[1].Length >= ZERO_WIDE_MAX)
								)
								||
								(
									Pulses[1].Length >= ZERO_THIN_MIN &&
									!(Pulses[0].Length < ZERO_WIDE_MIN || Pulses[0].Length >= ZERO_WIDE_MAX)
								)
							)
						)
						{
#ifdef SHOWLOGIC
							printf("0: %d %d\n", Pulses[0].Length, Pulses[1].Length);
#endif
							Target->Type = TE_BIT;
							Target->BaudRate = 0;
							Target->Data.Bit.Data8 = 0;
							Target->Data.Bit.Data32 = 0;
							Target->Length = Length;
							Target->Phase = Pulses[0].Phase;
							break;
						}

						/* condition under which we recognise a '1' */
						GetEvent(&Pulses[2], CONV_PULSE);
						GetEvent(&Pulses[3], CONV_PULSE);

						bool OneGood = false, OneBad = false;
						int c = 4;
						while(c--)
						{
							if(Pulses[c].Length < ONE_THIN_MAX)
								OneGood = true;
							if(Pulses[c].Length < ONE_WIDE_MIN && Pulses[c].Length >= ONE_WIDE_MAX)
								OneBad = false;
						}

						Length += Pulses[2].Length + Pulses[3].Length;
						if(
							Length >= WAVE_MIN && Length < WAVE_MAX &&
							OneGood && !OneBad
						)
						{
#ifdef SHOWLOGIC
							printf("1: %d %d %d %d\n", Pulses[0].Length, Pulses[1].Length, Pulses[2].Length, Pulses[3].Length);
#endif
							Target->Type = TE_BIT;
							Target->BaudRate = 0;
							Target->Data.Bit.Data8 = 1;
							Target->Data.Bit.Data32 = 0xf;
							Target->Length = Length;
							Target->Phase = Pulses[0].Phase;
							break;
						}

						/* default: gap, and push other pulses back into queue using Diversion */
#ifdef SHOWLOGIC
						printf("gap: %d %d %d %d\n", Pulses[0].Length, Pulses[1].Length, Pulses[2].Length, Pulses[3].Length);
#endif
						Target->Type = TE_GAP;
						Target->BaudRate = 0;
						Target->Length = Pulses[0].Length;
						Target->Phase = Pulses[0].Phase;

						TEHolder *Ptr = Diversion->Next;
						Ptr->Event = Pulses[1];
						Ptr = Ptr->Next;
						Ptr->Event = Pulses[2];
						Ptr = Ptr->Next;
						Ptr->Event = Pulses[3];

						Ptr->Next = (*ListPtr)->Next;
						ListPtr = &Diversion->Next->Next;
					} break;
				}
			break;
		}

		CursorTime += Target->Length;
		if(!(*ListPtr))
		{
			Seek(0);
		}
	}
}

char *TapeEventQueue::GetTitle()
{
	return NULL;
}

char *TapeEventQueue::GetCreator()
{
	return NULL;
}
