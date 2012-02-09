/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	DisplayUpdate.cpp
	=================

	Code to actually draw the display based on current tables and display
	memory

*/

#include "Display.h"
#include "6502.h"
#include <memory.h>

SDL_Surface *CDisplay::GetFrontBuffer()
{
	SDL_mutexP(FrameBufferMutex);
	return FrameBuffer;
}

void CDisplay::ReleaseFrontBuffer()
{
	SDL_mutexV(FrameBufferMutex);
}

/*void CDisplay::RestrictDisplay(SDL_Rect targ)
{
	Restricted = true;
	ScaleTarget = targ;

	if(!Overlay)
	{
		SDL_mutexP(FrameBufferMutex);
		BackBuffer = SDL_CreateRGBSurface(SDL_HWSURFACE, FrameBuffer->w, FrameBuffer->h, FrameBuffer->format->BitsPerPixel, FrameBuffer->format->Rmask, FrameBuffer->format->Gmask, FrameBuffer->format->Bmask, FrameBuffer->format->Amask);

		if(!BackBuffer)
			BackBuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, FrameBuffer->w, FrameBuffer->h, FrameBuffer->format->BitsPerPixel, FrameBuffer->format->Rmask, FrameBuffer->format->Gmask, FrameBuffer->format->Bmask, FrameBuffer->format->Amask);
		SDL_mutexV(FrameBufferMutex);
	}

	Iconify(false);
}

void CDisplay::EndRestriction()
{
	Restricted = false;
	if(BackBuffer)
	{
		SDL_mutexP(FrameBufferMutex);
		SDL_FreeSurface(BackBuffer);
		BackBuffer = NULL;
		SDL_mutexV(FrameBufferMutex);
	}

	ScaleTarget.y = 0;
	ScaleTarget.h = FrameBuffer->h;

	ScaleTarget.w = CORRECT_W(FrameBuffer->w);
	ScaleTarget.x = (FrameBuffer->w/2) - (ScaleTarget.w/2);

	Iconify(false);
}*/

#define	RTC_TIME	((FrameCount&1) ? 19904 : 19846)
#define END_TIME	((FrameCount&1) ? 40064 : 39936)
Uint32 CDisplay::Update(Uint32 CycleCount, bool Catchup)
{
	/* check if iconified */
	Uint8 Visible = SDL_GetAppState()&SDL_APPACTIVE;
	if(!Icon && !Visible) Iconify(true);
	if(Icon && Visible) Iconify(false);

	IScanline += CycleCount;
	CurrentTime += CycleCount;

	if(IScanline == RTC_TIME)
	{
//		printf("[%d] ", IScanline);
		ULA->AdjustInterrupts(CurrentTime, 0xff, ULAIRQ_RTC);
	}

	if(IScanline >= END_TIME)
	{
		/* set gather address base */
		((C6502 *)PPPtr->GetWellDefinedComponent(COMPONENT_CPU))->SetGathering(AddrSource, VideoBuffer32);

		/* set end of display interrupt */
		ULA->AdjustInterrupts(CurrentTime, 0xff, ULAIRQ_DISPLAY);

		/* draw display */
		UpdateDisplay(Catchup);

		/* generate anticipated address matrix for screen if necessary */
		if((BackupStartAddr ^ StartAddr) || ModeChanged)
		{
			BackupStartAddr = FrameStartAddr;
			FillAddressTable(CMode, 0);
		}

		ModeChanged = false;

//		printf("%d\n", IScanline);
		FrameStart = CurrentTime;
		FrameStartAddr = StartAddr;
		LineBase = 0;

		IScanline -= END_TIME;
		FrameCount++;
	}

	/* calculate time until next potential interrupt */
	if(IScanline < RTC_TIME)
		return RTC_TIME - IScanline;
	else
		return END_TIME - IScanline;
}
#undef RTC_TIME
#undef END_TIME

#define AddValueCRC(CRC, v) CRC = ((CRC << 8) | (v)) ^ CRCTable[CRC >> 24]
#define AddValue32CRC(CRC, v)\
	AddValueCRC(CRC, (v)&0xff);\
	AddValueCRC(CRC, ((v) >> 8)&0xff);\
	AddValueCRC(CRC, ((v) >> 16)&0xff);\
	AddValueCRC(CRC, (v) >> 24)

void CDisplay::NonAffectingDraw(SDL_Surface *Target)
{
	SDL_Surface *BackBuffer = NULL;
	if((Target->w != 640) || (Target->h != 256))
	{
		BackBuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 256, Target->format->BitsPerPixel, Target->format->Rmask, Target->format->Gmask, Target->format->Bmask, Target->format->Amask);
	}
	else
		BackBuffer = Target;

	/* now draw to BackBuffer */
	SDL_LockSurface(BackBuffer);
//	Uint8 *PelPtr = (Uint8 *)BackBuffer->pixels;
	for(int y = 0; y < 256; y++)
	{
		for(int x = 0; x < 640; x++)
		{
			/* something */
		}
	}

	/* software scale from BackBuffer to Target */
	if(BackBuffer != Target)
	{
		SDL_LockSurface(Target);

		int SrcY = 0;
		int AddX, AddY;

		AddX = (BackBuffer->w << 16) / ScaleTarget.w;
		AddY = (BackBuffer->h << 16) / ScaleTarget.h;

		switch(Target->format->BytesPerPixel)
		{
			case 1:{
				Uint8 *PelLine = &((Uint8 *)Target->pixels)[ScaleTarget.y*Target->pitch + ScaleTarget.x];

				int y = ScaleTarget.h;
				while(y--)
				{
					Uint8 *SrcLine = &((Uint8 *)BackBuffer->pixels)[(SrcY >> 16) * BackBuffer->pitch];
					int SrcX = 0;

					int x = ScaleTarget.w;
					while(x--)
					{
						*PelLine++ = SrcLine[SrcX >> 16];
						SrcX += AddX;
					}
					PelLine += Target->pitch - ScaleTarget.w;
					SrcY += AddY;
				}
			}break;

			case 2:{
				Uint16 *PelLine = (Uint16 *)&((Uint8 *)Target->pixels)[ScaleTarget.y*Target->pitch + (ScaleTarget.x << 1)];

				int y = ScaleTarget.h;
				while(y--)
				{
					Uint16 *SrcLine = (Uint16 *)&((Uint8 *)BackBuffer->pixels)[(SrcY >> 16) * BackBuffer->pitch];
					int SrcX = 0;

					int x = ScaleTarget.w;
					while(x--)
					{
						*PelLine++ = SrcLine[SrcX >> 16];
						SrcX += AddX;
					}
					PelLine = (Uint16 *)( (Uint8 *)PelLine + Target->pitch - (ScaleTarget.w << 1));
					SrcY += AddY;
				}
			}break;

			case 3:{
				Uint8 *PelLine = &((Uint8 *)Target->pixels)[ScaleTarget.y*Target->pitch + ScaleTarget.x*3];

				int y = ScaleTarget.h;
				while(y--)
				{
					Uint8 *SrcLine = &((Uint8 *)BackBuffer->pixels)[(SrcY >> 16) * BackBuffer->pitch];
					int SrcX = 0;

					int x = ScaleTarget.w;
					while(x--)
					{
						PelLine[0] = SrcLine[(SrcX >> 16)*3 + 0];
						PelLine[1] = SrcLine[(SrcX >> 16)*3 + 1];
						PelLine[2] = SrcLine[(SrcX >> 16)*3 + 2];
						PelLine += 3;
						SrcX += AddX;
					}
					PelLine += Target->pitch - (ScaleTarget.w*3);
					SrcY += AddY;
				}
			}break;

			case 4:{
				Uint32 *PelLine = (Uint32 *)&((Uint8 *)Target->pixels)[ScaleTarget.y*Target->pitch + (ScaleTarget.x << 2)];

				int y = ScaleTarget.h;
				while(y--)
				{
					Uint32 *SrcLine = (Uint32 *)&((Uint8 *)BackBuffer->pixels)[(SrcY >> 16) * BackBuffer->pitch];
					int SrcX = 0;

					int x = ScaleTarget.w;
					while(x--)
					{
						*PelLine++ = SrcLine[SrcX >> 16];
						SrcX += AddX;
					}
					PelLine = (Uint32 *)( (Uint8 *)PelLine + Target->pitch - (ScaleTarget.w << 2));
					SrcY += AddY;
				}
			}break;
		}

		SDL_UnlockSurface(Target);
		SDL_UnlockSurface(BackBuffer);
		SDL_FreeSurface(BackBuffer);
	}
	else
		SDL_UnlockSurface(BackBuffer);
}

#define AccessVideo8(addr) ((Uint8 *)(VideoBuffer32))[((addr) << 2) + (VideoOffsets8[addr]&3)]

void CDisplay::UpdateDisplay(bool Catchup)
{
	Uint8 *Pixels = NULL;
	int Pitch = 0;

	if(Icon || !FrameBuffer) Catchup = true;

	if(!Catchup)
	{
		if(Overlay)
		{
			Catchup = SDL_LockYUVOverlay(Overlay) ? true : false;
			Pixels = (Uint8 *)Overlay->pixels[0];
			Pitch = Overlay->pitches[0];
		}
		else
		{
			SDL_mutexP(FrameBufferMutex);

			Catchup = SDL_LockSurface(FrameBuffer) ? true : false;
			Pixels = (Uint8 *)FrameBuffer->pixels;
			Pitch = FrameBuffer->pitch;
		}
	}

	if(!Catchup)
	{
		/* enact all changes that occurred before pixels */
		while(
				(VideoWritePtr != VideoReadPtr) &&
				((Uint32)(VideoEventQueue[VideoReadPtr].TimeStamp - FrameStart) < DISPLAY_START)
			)
			EnactEvent();
		SumEvents();

		FrameStart += DISPLAY_START;

		Uint8 *bdptr1 = Pixels, *dptr1;

		if(!Overlay) bdptr1 += YOffset*Pitch + XOffset;

		Uint8 *bdptr2 = bdptr1 + Pitch, *dptr2;
		int Addr;
		bool Dirty[257];

		for(int y = 0; y < 256; y++)
		{
			bool EventThisLine;

			EventThisLine = 
				(VideoWritePtr != VideoReadPtr) &&
				((Uint32)(VideoEventQueue[VideoReadPtr].TimeStamp - FrameStart) < 128);

			Addr = DISPLAY_START + (y << 7);
			dptr1 = bdptr1;
			dptr2 = bdptr2;

			if(EmptyLine[y] | (Blank && !EventThisLine))
			{
				/* fill in empty line, but check here that the line is free of events! */
				Uint32 Colour = EmptyLine[y] ? Black : BlankColour;

				if(Colour != CRCBuffer[y])
				{	
					CRCBuffer[y] = Colour;
					Dirty[y] = true;

					if(Overlay)
					{
						Uint32 *dptr132 = (Uint32 *)dptr1;

						int c = Overlay->w >> 1;
						while(c--)
							*dptr132++ = Colour;
					}
					else /* consider SDL_FillRect here instead? */
						switch(FrameBuffer->format->BytesPerPixel)
						{
							case 1:
								memset(dptr1, Colour, 640);
								memset(dptr2, Colour, 640);
							break;

							case 2:
								Colour |= (Colour << 16);
							case 4: {

								Uint32 *dptr132 = (Uint32 *)dptr1;
								Uint32 *dptr232 = (Uint32 *)dptr2;

								int c = (FrameBuffer->format->BytesPerPixel == 4) ? 640 : 320;
								while(c--)
								{
									*dptr132++ = Colour;
									*dptr232++ = Colour;
								}
							}break;

							case 3:{
								Uint32 OutputWords[3];

#ifdef SDL_LIL_ENDIAN
								OutputWords[0] = Colour | Colour << 24;
								OutputWords[1] = (Colour >> 8) | (Colour << 16);
								OutputWords[2] = (Colour >> 16) | (Colour << 8);
#else
								OutputWords[0] = (Colour << 8) | (Colour >> 24);
								OutputWords[1] = (Colour << 16) | (Colour >> 8);
								OutputWords[2] = (Colour << 24) | (Colour >> 8);
#endif

								int c = 160;
								while(c--)
								{
									((Uint32 *)dptr1)[0] = OutputWords[0];
									((Uint32 *)dptr1)[1] = OutputWords[1];
									((Uint32 *)dptr1)[2] = OutputWords[2];
									((Uint32 *)dptr2)[0] = OutputWords[0];
									((Uint32 *)dptr2)[1] = OutputWords[1];
									((Uint32 *)dptr2)[2] = OutputWords[2];

									dptr1 += 12;
									dptr2 += 12;
								}
							}break;
						}
				}
				else
					Dirty[y] = false;

				Addr += 128;

				while( 
						(VideoWritePtr != VideoReadPtr) &&
						((Uint32)(VideoEventQueue[VideoReadPtr].TimeStamp - FrameStart) < 128)
					)
					EnactEvent();
				SumEvents();

				FrameStart += 128;
			}
			else
			{
				/* calculate CEC32 of palette + mode + bytes. Is it different? */
				Uint32 NewCRC;
				int c;

				NewCRC = 0xffffffff;
				AddValueCRC(NewCRC, CMode);

				switch(CMode)
				{
					default:
						AddValueCRC(NewCRC, PaletteBytes[0]);
						AddValueCRC(NewCRC, PaletteBytes[1]);
					break;

					case 2 :
						c = 8;
						while(c--)
							AddValueCRC(NewCRC, PaletteBytes[c]);
					break;
				}

				c = 80;
				if(DisplayMultiplexed)
				{
					while(c--)
						AddValue32CRC(NewCRC, VideoBuffer32[c+Addr]);
				}
				else
				{
					while(c--)
						AddValueCRC(NewCRC, AccessVideo8(c+Addr));
				}

				/* preview events */
				Uint32 VideoPreviewPtr = VideoReadPtr;
				while(	(VideoWritePtr != VideoPreviewPtr) &&
						((Uint32)(VideoEventQueue[VideoPreviewPtr].TimeStamp - FrameStart) < 80)
					)
					{
						AddValueCRC(NewCRC, (Uint32)(VideoEventQueue[VideoPreviewPtr].TimeStamp - FrameStart));
						AddValueCRC(NewCRC, VideoEventQueue[VideoPreviewPtr].Value);
						AddValueCRC(NewCRC, VideoEventQueue[VideoPreviewPtr].Address);
						AddValueCRC(NewCRC, VideoEventQueue[VideoPreviewPtr].Type);

						VideoPreviewPtr = (VideoPreviewPtr+1)&(CDISPLAY_VIDEOEVENT_LENGTH-1);
					}

				if(NewCRC != CRCBuffer[y])
				{
					Dirty[y] = true;
					CRCBuffer[y] = NewCRC;

					int lowpart, lowmask = (1 << (TableShift-1));
					lowpart = 0;
					Uint32 c = 80;
					Uint32 CyclesToRun;

					while(c)
					{
						/* enact latest events */
						while(	(VideoWritePtr != VideoReadPtr) &&
								(VideoEventQueue[VideoReadPtr].TimeStamp == FrameStart)
							)
							EnactEvent();
						SumEvents();

						/* calculate number of cycles to next event */
						if(VideoWritePtr == VideoReadPtr)
							CyclesToRun = c;
						else
						{
							CyclesToRun = VideoEventQueue[VideoReadPtr].TimeStamp - FrameStart;

							if(CyclesToRun > c)
								CyclesToRun = c;
						}

						/* plot pixels */
						c -= CyclesToRun;
						FrameStart += CyclesToRun;
						if(DisplayMultiplexed)
						{
							/* and here it gets ugly - can't really justify using memcpy for an average of 4 bytes a pop */
							Uint32 CurrentByte;

							/* switch statement acts on data in following form:

									obbbmmm

									o = 1 if overlay in use, 0 otherwise
									bbb = bit depth (1 - 4)
									mmm = graphics mode
							*/
							switch(CMode | (FrameBuffer->format->BytesPerPixel << 3) | (Overlay ? 64 : 0))
							{
#define Wrapper4bpp80(t, w)\
	while(CyclesToRun--)\
	{\
		CurrentByte = VideoBuffer32[Addr&ByteMask];\
		Addr ++;\
		/* 4bpp transposition */\
		CurrentByte =	((CurrentByte&0x0000000f))			| ((CurrentByte&0x00000f00) >> 4)	|\
						((CurrentByte&0x000f0000) >> 8)		| ((CurrentByte&0x0f000000) >> 12)	|\
						((CurrentByte&0x000000f0) << 12)	| ((CurrentByte&0x0000f000) << 8)	|\
						((CurrentByte&0x00f00000) << 4)		| ((CurrentByte&0xf0000000));\
\
		int pels = 4;\
		while(pels--)\
		{\
			t;\
			CurrentByte <<= 8;\
		}\
	}

#define Wrapper2bpp80(t, w)\
	while(CyclesToRun--)\
	{\
		CurrentByte = VideoBuffer32[Addr&ByteMask];\
		Addr ++;\
		/* 2bpp transposition */\
		CurrentByte =	((CurrentByte&0x0000000f))			| ((CurrentByte&0x000f0000) >> 12)	|\
						((CurrentByte&0x000000f0) << 4)		| ((CurrentByte&0x00f00000) >> 8)	|\
						((CurrentByte&0x00000f00) << 8)		| ((CurrentByte&0x0f000000) >> 4)	|\
						((CurrentByte&0x0000f000) << 12)	| ((CurrentByte&0xf0000000));\
\
		int pels = 4;\
		while(pels--)\
		{\
			t;\
			CurrentByte <<= 8;\
		}\
	}

#define Wrapper2bpp40(t, w)\
	CyclesToRun >>= 1;\
	while(CyclesToRun--)\
	{\
		CurrentByte = VideoBuffer32[Addr&ByteMask];\
		Addr += 2;\
		/* 2bpp transposition */\
		CurrentByte =	((CurrentByte&0x0000000f))			| ((CurrentByte&0x000f0000) >> 12)	|\
						((CurrentByte&0x000000f0) << 4)		| ((CurrentByte&0x00f00000) >> 8)	|\
						((CurrentByte&0x00000f00) << 8)		| ((CurrentByte&0x0f000000) >> 4)	|\
						((CurrentByte&0x0000f000) << 12)	| ((CurrentByte&0xf0000000));\
\
		int pels = 4;\
		while(pels--)\
		{\
			w;\
			CurrentByte <<= 8;\
		}\
	}

#define Wrapper1bpp40(t, w)\
	CyclesToRun >>= 1;\
	while(CyclesToRun--)\
	{\
		CurrentByte = VideoBuffer32[Addr&ByteMask];\
		Addr += 2;\
		int pels = 4;\
		while(pels--)\
		{\
			w;\
			CurrentByte <<= 8;\
		}\
	}

#define Wrapper1bpp80(t, w)\
	while(CyclesToRun--)\
	{\
		CurrentByte = VideoBuffer32[Addr&ByteMask];\
		Addr ++;\
		int pels = 4;\
		while(pels--)\
		{\
			t;\
			CurrentByte <<= 8;\
		}\
	}

#define SPelGroup(mode, Wrapper)\
	case mode | (1 << 3): /* 8bpp */\
		Wrapper(\
			*((Uint16 *)dptr1) = *(Uint16 *)(&MultiDisplayTables[(CurrentByte >> 24) << 2]); dptr1 += 2;\
			*((Uint16 *)dptr2) = *(Uint16 *)(&MultiDisplayTables[(CurrentByte >> 24) << 2]); dptr2 += 2;,\
\
			*((Uint32 *)dptr1) = *(Uint16 *)(&MultiDisplayTables[(CurrentByte >> 24) << 2]); dptr1 += 4;\
			*((Uint32 *)dptr2) = *(Uint16 *)(&MultiDisplayTables[(CurrentByte >> 24) << 2]); dptr2 += 4;\
		);\
	break;\
	case mode | (2 << 3): /* 16bpp */\
		Wrapper(\
			*((Uint32 *)dptr1) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr1 += 4;\
			*((Uint32 *)dptr2) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr2 += 4;,\
\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr1 += 8;\
			*((Uint64 *)dptr2) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr2 += 8;\
		);\
	break;\
	case mode | (3 << 3): /* 24bpp (yuck) */\
		Wrapper(\
			*((Uint32 *)dptr1) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr1 += 4;\
			*((Uint16 *)dptr1) = *(Uint16 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+4]); dptr1 += 2;\
			*((Uint32 *)dptr2) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr2 += 4;\
			*((Uint16 *)dptr2) = *(Uint16 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+4]); dptr2 += 2;,\
\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr1 += 8;\
			*((Uint32 *)dptr1) = *(Uint32 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+8]); dptr1 += 4;\
			*((Uint64 *)dptr2) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr2 += 8;\
			*((Uint32 *)dptr2) = *(Uint32 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+8]); dptr2 += 4;\
		);\
	break;\
	case mode | (4 << 3): /* 32bpp */\
		Wrapper(\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr1 += 8;\
			*((Uint64 *)dptr2) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr2 += 8;,\
\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr1 += 8;\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+8]); dptr1 += 8;\
			*((Uint64 *)dptr2) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr2 += 8;\
			*((Uint64 *)dptr2) = *(Uint64 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+8]); dptr2 += 8;\
		);\
	break;\
	case mode | (1 << 3) | 64: /* overlay surface */\
	case mode | (2 << 3) | 64:\
	case mode | (3 << 3) | 64:\
	case mode | (4 << 3) | 64:\
		Wrapper(\
			*((Uint32 *)dptr1) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr1 += 4;,\
\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr1 += 8;\
		);\
	break;

#define DPelGroup(mode1, mode2, Wrapper)\
	case mode1 | (1 << 3): /* 8bpp */\
	case mode2 | (1 << 3):\
		Wrapper(\
			*((Uint16 *)dptr1) = *(Uint16 *)(&MultiDisplayTables[(CurrentByte >> 24) << 2]); dptr1 += 2;\
			*((Uint16 *)dptr2) = *(Uint16 *)(&MultiDisplayTables[(CurrentByte >> 24) << 2]); dptr2 += 2;,\
\
			*((Uint32 *)dptr1) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 2]); dptr1 += 4;\
			*((Uint32 *)dptr2) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 2]); dptr2 += 4;\
		);\
	break;\
	case mode1 | (2 << 3): /* 16bpp */\
	case mode2 | (2 << 3):\
		Wrapper(\
			*((Uint32 *)dptr1) = *(Uint16 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr1 += 4;\
			*((Uint32 *)dptr2) = *(Uint16 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr2 += 4;,\
\
			*((Uint64 *)dptr1) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr1 += 8;\
			*((Uint64 *)dptr2) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr2 += 8;\
		);\
	break;\
	case mode1 | (3 << 3): /* 24bpp (yuck) */\
	case mode2 | (3 << 3):\
		Wrapper(\
			*((Uint32 *)dptr1) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr1 += 4;\
			*((Uint16 *)dptr1) = *(Uint16 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+4]); dptr1 += 2;\
			*((Uint32 *)dptr2) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr2 += 4;\
			*((Uint16 *)dptr2) = *(Uint16 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+4]); dptr2 += 2;,\
\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr1 += 8;\
			*((Uint32 *)dptr1) = *(Uint32 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+8]); dptr1 += 4;\
			*((Uint64 *)dptr2) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr2 += 8;\
			*((Uint32 *)dptr2) = *(Uint32 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+8]); dptr2 += 4;\
		);\
	break;\
	case mode1 | (4 << 3): /* 32bpp */\
	case mode2 | (4 << 3):\
		Wrapper(\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr1 += 8;\
			*((Uint64 *)dptr2) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr2 += 8;,\
\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr1 += 8;\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+8]); dptr1 += 8;\
			*((Uint64 *)dptr2) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 4]); dptr2 += 8;\
			*((Uint64 *)dptr2) = *(Uint64 *)(&MultiDisplayTables[((CurrentByte >> 24) << 4)+8]); dptr2 += 8;\
		);\
	break;\
	case mode1 | (1 << 3) | 64: /* overlay surface */\
	case mode1 | (2 << 3) | 64:\
	case mode1 | (3 << 3) | 64:\
	case mode1 | (4 << 3) | 64:\
	case mode2 | (1 << 3) | 64:\
	case mode2 | (2 << 3) | 64:\
	case mode2 | (3 << 3) | 64:\
	case mode2 | (4 << 3) | 64:\
		Wrapper(\
			*((Uint32 *)dptr1) = *(Uint32 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr1 += 4;,\
\
			*((Uint64 *)dptr1) = *(Uint64 *)(&MultiDisplayTables[(CurrentByte >> 24) << 3]); dptr1 += 8;\
		);\
	break;

								SPelGroup(1, Wrapper2bpp80);	/* Mode 1 */
								SPelGroup(5, Wrapper2bpp40);	/* Mode 5 */
								SPelGroup(2, Wrapper4bpp80);	/* Mode 2 */

								DPelGroup(0, 3, Wrapper1bpp80);	/* Modes 0 and 3 */
								DPelGroup(4, 6, Wrapper1bpp40);	/* Modes 4 and 6 */
							}
						}
						else
						{
							if(Overlay)
							{
								/* 2 bytes per pixel always... when on a machine that supports overlays check this! */
								while(CyclesToRun--)
								{
									memcpy(dptr1, &DisplayTables[lowpart | (AccessVideo8(Addr&ByteMask) << TableShift)], TableEntryLength); dptr1 += TableEntryLength;
									Addr++;
									lowpart ^= lowmask;
								}
							}
							else
							{
								/* TableEntryLength = bpp*8 */
								switch(TableEntryLength)
								{
									default:
										while(CyclesToRun--)
										{
											memcpy(dptr2, &DisplayTables[lowpart | (AccessVideo8(Addr&ByteMask) << TableShift)], TableEntryLength); dptr2 += TableEntryLength;
											memcpy(dptr1, &DisplayTables[lowpart | (AccessVideo8(Addr&ByteMask) << TableShift)], TableEntryLength); dptr1 += TableEntryLength;
											Addr++;
											lowpart ^= lowmask;
										}
									break;

									case 8:
										while(CyclesToRun--)
										{
											Uint32 *BasePtr = &DisplayTables[lowpart | (AccessVideo8(Addr&ByteMask) << TableShift)];
											((Uint32 *)dptr1)[0] = ((Uint32 *)dptr2)[0] = BasePtr[0];
											((Uint32 *)dptr1)[1] = ((Uint32 *)dptr2)[1] = BasePtr[1];
											dptr2 += 8;
											dptr1 += 8;
											Addr++;
											lowpart ^= lowmask;
										}
									break;

									case 16:
										while(CyclesToRun--)
										{
											Uint32 *BasePtr = &DisplayTables[lowpart | (AccessVideo8(Addr&ByteMask) << TableShift)];
											((Uint32 *)dptr1)[0] = ((Uint32 *)dptr2)[0] = BasePtr[0];
											((Uint32 *)dptr1)[1] = ((Uint32 *)dptr2)[1] = BasePtr[1];
											((Uint32 *)dptr1)[2] = ((Uint32 *)dptr2)[2] = BasePtr[2];
											((Uint32 *)dptr1)[3] = ((Uint32 *)dptr2)[3] = BasePtr[3];
											dptr2 += 16;
											dptr1 += 16;
											Addr++;
											lowpart ^= lowmask;
										}
									break;

									case 24:
										while(CyclesToRun--)
										{
											Uint32 *BasePtr = &DisplayTables[lowpart | (AccessVideo8(Addr&ByteMask) << TableShift)];
											((Uint32 *)dptr1)[0] = ((Uint32 *)dptr2)[0] = BasePtr[0];
											((Uint32 *)dptr1)[1] = ((Uint32 *)dptr2)[1] = BasePtr[1];
											((Uint32 *)dptr1)[2] = ((Uint32 *)dptr2)[2] = BasePtr[2];
											((Uint32 *)dptr1)[3] = ((Uint32 *)dptr2)[3] = BasePtr[3];
											((Uint32 *)dptr1)[4] = ((Uint32 *)dptr2)[4] = BasePtr[4];
											((Uint32 *)dptr1)[5] = ((Uint32 *)dptr2)[5] = BasePtr[5];
											dptr2 += 24;
											dptr1 += 24;
											Addr++;
											lowpart ^= lowmask;
										}
									break;

									case 32:
										while(CyclesToRun--)
										{
											Uint32 *BasePtr = &DisplayTables[lowpart | (AccessVideo8(Addr&ByteMask) << TableShift)];
											Addr++;
											lowpart ^= lowmask;
											((Uint32 *)dptr1)[0] = BasePtr[0];
											((Uint32 *)dptr1)[1] = BasePtr[1];
											((Uint32 *)dptr1)[2] = BasePtr[2];
											((Uint32 *)dptr1)[3] = BasePtr[3];
											((Uint32 *)dptr1)[4] = BasePtr[4];
											((Uint32 *)dptr1)[5] = BasePtr[5];
											((Uint32 *)dptr1)[6] = BasePtr[6];
											((Uint32 *)dptr1)[7] = BasePtr[7];
											dptr1 += 32;
											((Uint32 *)dptr2)[0] = BasePtr[0];
											((Uint32 *)dptr2)[1] = BasePtr[1];
											((Uint32 *)dptr2)[2] = BasePtr[2];
											((Uint32 *)dptr2)[3] = BasePtr[3];
											((Uint32 *)dptr2)[4] = BasePtr[4];
											((Uint32 *)dptr2)[5] = BasePtr[5];
											((Uint32 *)dptr2)[6] = BasePtr[6];
											((Uint32 *)dptr2)[7] = BasePtr[7];
											dptr2 += 32;
										}
									break;
								}
							}
						}
					}

					/* enact remaining events to end of scanline */
					while( 
							(VideoWritePtr != VideoReadPtr) &&
							((Uint32)(VideoEventQueue[VideoReadPtr].TimeStamp - FrameStart) < 48)
						)
						EnactEvent();
					SumEvents();

					Addr += 48;
					FrameStart += 48;
				}
				else
				{
					Dirty[y] = false;

					/* enact scanline events */
					while( 
							(VideoWritePtr != VideoReadPtr) &&
							((Uint32)(VideoEventQueue[VideoReadPtr].TimeStamp - FrameStart) < 128)
						)
						EnactEvent();
					SumEvents();

					Addr += 128;
					FrameStart += 128;
				}

			}

			if(Overlay)
				bdptr1 += Pitch;
			else
			{
				bdptr1 += Pitch << 1;
				bdptr2 += Pitch << 1;
			}
		}

		if(Overlay)
		{
			SDL_UnlockYUVOverlay(Overlay);
			SDL_DisplayYUVOverlay(Overlay, &ScaleTarget);
		}
		else
		{
/*			if(BackBuffer)
			{
				SDL_LockSurface(FrameBuffer);

				int SrcY = 0;
				int AddX, AddY;

				AddX = (BackBuffer->w << 16) / ScaleTarget.w;
				AddY = (BackBuffer->h << 16) / ScaleTarget.h;

				switch(FrameBuffer->format->BytesPerPixel)
				{
					case 1:{
						Uint8 *PelLine = &((Uint8 *)FrameBuffer->pixels)[ScaleTarget.y*FrameBuffer->pitch + ScaleTarget.x];

						int y = ScaleTarget.h;
						while(y--)
						{
							Uint8 *SrcLine = &((Uint8 *)BackBuffer->pixels)[(SrcY >> 16) * BackBuffer->pitch];
							int SrcX = 0;

							int x = ScaleTarget.w;
							while(x--)
							{
								*PelLine++ = SrcLine[SrcX >> 16];
								SrcX += AddX;
							}
							PelLine += FrameBuffer->pitch - ScaleTarget.w;
							SrcY += AddY;
						}
					}break;

					case 2:{
						Uint16 *PelLine = (Uint16 *)&((Uint8 *)FrameBuffer->pixels)[ScaleTarget.y*FrameBuffer->pitch + (ScaleTarget.x << 1)];

						int y = ScaleTarget.h;
						while(y--)
						{
							Uint16 *SrcLine = (Uint16 *)&((Uint8 *)BackBuffer->pixels)[(SrcY >> 16) * BackBuffer->pitch];
							int SrcX = 0;

							int x = ScaleTarget.w;
							while(x--)
							{
								*PelLine++ = SrcLine[SrcX >> 16];
								SrcX += AddX;
							}
							PelLine = (Uint16 *)( (Uint8 *)PelLine + FrameBuffer->pitch - (ScaleTarget.w << 1));
							SrcY += AddY;
						}
					}break;

					case 3:{
						Uint8 *PelLine = &((Uint8 *)FrameBuffer->pixels)[ScaleTarget.y*FrameBuffer->pitch + ScaleTarget.x*3];

						int y = ScaleTarget.h;
						while(y--)
						{
							Uint8 *SrcLine = &((Uint8 *)BackBuffer->pixels)[(SrcY >> 16) * BackBuffer->pitch];
							int SrcX = 0;

							int x = ScaleTarget.w;
							while(x--)
							{
								PelLine[0] = SrcLine[(SrcX >> 16)*3 + 0];
								PelLine[1] = SrcLine[(SrcX >> 16)*3 + 1];
								PelLine[2] = SrcLine[(SrcX >> 16)*3 + 2];
								PelLine += 3;
								SrcX += AddX;
							}
							PelLine += FrameBuffer->pitch - (ScaleTarget.w*3);
							SrcY += AddY;
						}
					}break;

					case 4:{
						Uint32 *PelLine = (Uint32 *)&((Uint8 *)FrameBuffer->pixels)[ScaleTarget.y*FrameBuffer->pitch + (ScaleTarget.x << 2)];

						int y = ScaleTarget.h;
						while(y--)
						{
							Uint32 *SrcLine = (Uint32 *)&((Uint8 *)BackBuffer->pixels)[(SrcY >> 16) * BackBuffer->pitch];
							int SrcX = 0;

							int x = ScaleTarget.w;
							while(x--)
							{
								*PelLine++ = SrcLine[SrcX >> 16];
								SrcX += AddX;
							}
							PelLine = (Uint32 *)( (Uint8 *)PelLine + FrameBuffer->pitch - (ScaleTarget.w << 2));
							SrcY += AddY;
						}
					}break;
				}

				SDL_UnlockSurface(FrameBuffer);
				SDL_UpdateRect(FrameBuffer, 0, 0, 0, 0);
				SDL_UnlockSurface(BackBuffer);
				SDL_mutexV(FrameBufferMutex);
			}
			else
			{*/

				SDL_UnlockSurface(FrameBuffer);

				/* and now UpdateRect calls are necessary to make sure all scanlines appear on
				display */
				int StartY = 0, EndY = 0;
				while(EndY < 512)
				{
					StartY = EndY;
					while(!Dirty[StartY >> 1] && (StartY < 512)) StartY+=2;
					if(StartY == 512) break;

					EndY = StartY;
					while(Dirty[EndY >> 1] && (EndY < 512)) EndY+=2;

					SDL_UpdateRect(FrameBuffer, XOffset, YOffset+StartY, 640, EndY-StartY);
				}
				SDL_mutexV(FrameBufferMutex);
			//}
		}
	}

	// clear any events that occur after end of visible pixels (which may be all events if we didn't draw this frame... */
	while(VideoReadPtr != VideoWritePtr)
		EnactEvent();
	SumEvents();
}

/* CRC stuff */

/* build CRCTable */
Uint32 CDisplay::CRCTable[256];
void CDisplay::SetupCRCTable()
{
	int bc;
	Uint32 crctemp;

	int c = 256;
	while(c--)
	{
		crctemp = c << 24;
		bc = 8;

		while(bc--)
		{
			if(crctemp&0x80000000)
			{
				crctemp = (crctemp << 1) ^ 0x04c11db7; /*CRC 32 polynomial */
			}
			else
			{
				crctemp <<= 1;
			}
		}

		CRCTable[c] = crctemp;
	}
}
