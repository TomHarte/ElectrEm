/*

	ElectrEm (c) 2000-6 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	Display.cpp
	===========

	Miscellaneous parts of code related to screen display - mostly to do
	with SDL interfacing, but also some limited table setup stuff
	
	TODO: fix palette reading for state snapshot stuff

*/

#include "Configuration/ConfigurationStore.h"
#include "Display.h"
#include "ProcessPool.h"
#include "Misc.h"
#include <stdlib.h>
#include <memory.h>
#include "6502.h"
/*
	In PAL terms, 52us of scanline time is visible, for 288 lines.
	
	Elk uses 256/288 lines, which is 8/9ths of total.
	It also uses 40/52 us, which is 10/13ths of the total.

	=> 9/8*10/13 = 45/52 of width should be used if display is full height
*/
#define CORRECT_W(w)	(w*45)/52

CDisplay::CDisplay( ElectronConfiguration &cfg )
{
	const SDL_VideoInfo *DesktopInfo = SDL_GetVideoInfo();
	NativeBytesPerPixel = DesktopInfo->vfmt->BytesPerPixel;

	SetupCRCTable();

	SDL_WM_SetCaption(WINDOW_TITLE, NULL);

	DisplayTables = NULL;
	MultiDisplayTables = NULL;
	Overlay = NULL;
	FrameBuffer = NULL;
	Surface = false;

	/* default settings */
	AllowOverlay = cfg.Display.AllowOverlay;

	DispFlags = 0;
	if ( cfg.Display.StartFullScreen )
	{
		DispFlags |= SDL_FULLSCREEN;
	}	
	
	DisplayMultiplexed = false;//cfg.Display.DisplayMultiplexed;
	
	BackupStartAddr = StartAddr = FrameStartAddr = 0;

	VideoWritePtr = VideoReadPtr = 0;
	MostRecentMode = CMode = 0;
	LineBase = 0;

	int c = 256;
	while(c--)
		PaletteColours[c] = c;

	/* thread related stuff */
	FrameBufferMutex = SDL_CreateMutex();
}

CDisplay::~CDisplay()
{
	/* this fixes some window placement issues under win 2000 */
#ifdef WIN32
	if(DispFlags&SDL_FULLSCREEN)
		ToggleFullScreen();
#endif

	if(Overlay) SDL_FreeYUVOverlay(Overlay);
	SDL_DestroyMutex(FrameBufferMutex);
	delete[] DisplayTables;
	delete[] MultiDisplayTables;
}

SDL_Surface * CDisplay::GetBufferCopy()
{
	SDL_Surface * newBuffer = NULL;
	SDL_PixelFormat fmt;
	memset( (void*)&fmt, 0, sizeof( SDL_PixelFormat ) );
	fmt.BitsPerPixel = 24;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	fmt.Rmask = 0x0000ff;
	fmt.Gmask = 0x00ff00;
	fmt.Bmask = 0xff0000;
#else
	fmt.Rmask = 0xff0000;
	fmt.Gmask = 0x00ff00;
	fmt.Bmask = 0x0000ff;
#endif

	newBuffer = SDL_ConvertSurface( GetFrontBuffer(), 
		&fmt,
		SDL_SWSURFACE);
	ReleaseFrontBuffer();
	return newBuffer;
}

void CDisplay::SetFlags(Uint32 Flags)
{
	bool ChangeRequired = false;

	if(((Flags&CDF_OVERLAY) ? true: false) != AllowOverlay)
	{
//		fprintf( stderr, "Overlay state changed\n" );
		AllowOverlay = (Flags&CDF_OVERLAY) ? true: false;
		ChangeRequired = true;
	}

	if(((Flags&CDF_MULTIPLEXED) ? true: false) != DisplayMultiplexed)
	{
		DisplayMultiplexed = (Flags&CDF_MULTIPLEXED) ? true: false;
		ChangeRequired = true;
	}

	if(((Flags&CDF_FULLSCREEN) ? true : false) != ((DispFlags&SDL_FULLSCREEN) ? true : false))
	{
		DispFlags ^= SDL_FULLSCREEN;
		ChangeRequired = true;
	}

	if(ChangeRequired)
	{
		FreeSurface();
		GetSurface();
	}
}

Uint32 CDisplay::GetFlags()
{
	return	(AllowOverlay ? CDF_OVERLAY : 0) | 
			(DisplayMultiplexed ? CDF_MULTIPLEXED : 0) |
			((DispFlags&SDL_FULLSCREEN) ? CDF_FULLSCREEN : 0);
}

void CDisplay::Iconify(bool icon)
{
	Icon = icon;
//	printf("iconify %s\n", icon? "true": "false"); fflush(stdout);

	if(!icon) /* implies window has been restored from minimised - clean frame buffer and set FrameBuffer for a full redraw */
	{
		GetSurface();

		if(Overlay)
		{
			if(!SDL_LockYUVOverlay(Overlay))
			{
				for(int y = 0; y < Overlay->h; y++)
				{
					int x = Overlay->w;
					while(x--)
					{
						switch(Overlay->format)
						{
							case SDL_UYVY_OVERLAY:
								((Uint8 *)Overlay->pixels[0])[(y*Overlay->pitches[0]) + (x << 1)] = 128;
								((Uint8 *)Overlay->pixels[0])[(y*Overlay->pitches[0]) + (x << 1) + 1] = 16;
							break;

							case SDL_YVYU_OVERLAY:
							case SDL_YUY2_OVERLAY:
								((Uint8 *)Overlay->pixels[0])[(y*Overlay->pitches[0]) + (x << 1)] = 16;
								((Uint8 *)Overlay->pixels[0])[(y*Overlay->pitches[0]) + (x << 1) + 1] = 128;
							break;
						}
					}
				}

				SDL_UnlockYUVOverlay(Overlay);
			}
		}

		if(FrameBuffer)
		{
			SDL_FillRect(FrameBuffer, NULL, SDL_MapRGB(FrameBuffer->format, 0, 0, 0));
			SDL_UpdateRect(FrameBuffer, 0, 0, 0, 0);
		}

		for(int y = 0; y < 256; y++) CRCBuffer[y] = 0;
	}
	else
	{
		/* release overlay, and reclaim it when iconification ends - task switching fix */
//		FreeSurface();
	}

//	printf("iconify done\n"); fflush(stdout);
}

bool CDisplay::TryForMode(int w, int h, int bpp, Uint32 flags)
{
//	printf( "Try for Mode %d %d %d 0x%x\n", w, h, bpp, flags );

	if(Overlay) 
	{
		SDL_FreeYUVOverlay(Overlay); 
		Overlay = NULL; 
	}

	if(bpp == 8)
	{
		FrameBuffer = SDL_SetVideoMode(w, h, bpp, flags | SDL_HWSURFACE | SDL_HWPALETTE);
		if(!FrameBuffer) FrameBuffer = SDL_SetVideoMode(w, h, bpp, (flags | SDL_HWSURFACE));
		if(!FrameBuffer) FrameBuffer = SDL_SetVideoMode(w, h, bpp, (flags | SDL_HWPALETTE));
		if(!FrameBuffer) FrameBuffer = SDL_SetVideoMode(w, h, 0, flags);
	}
	else
	{
		FrameBuffer = SDL_SetVideoMode(w, h, bpp, flags | SDL_HWSURFACE);
		if(!FrameBuffer) FrameBuffer = SDL_SetVideoMode(w, h, bpp, flags);
		if(!FrameBuffer) FrameBuffer = SDL_SetVideoMode(w, h, 0, flags);
	}

//	if(FrameBuffer) printf("flags: %08x, Bpp:%d\n", FrameBuffer->flags, FrameBuffer->format->BytesPerPixel);
	return FrameBuffer ? true : false;
}

/* MSVC 6 vs 7 related... */
#ifndef _ftol2
extern "C" long _ftol2( double dblSource ) { return (long)dblSource; }
#endif

void CDisplay::GetSurface()
{
	if(Surface) return;

	FreeSurface();

	/* try to get an overlay surface first */
	if( AllowOverlay && 
		DispFlags&SDL_FULLSCREEN && 
		TryForMode( (DispFlags&SDL_FULLSCREEN) ? 1024 : 640,
		            (DispFlags&SDL_FULLSCREEN) ? 768 : 512,
					32,
					DispFlags))
	{
		Uint32 OverlayTypes[] =
		{
			SDL_UYVY_OVERLAY,
			SDL_YUY2_OVERLAY,
			SDL_YVYU_OVERLAY,
			0
		};
		int OverlayPtr = 0;

		while(!Overlay && OverlayTypes[OverlayPtr])
		{
			if(Overlay = SDL_CreateYUVOverlay(640, 256, OverlayTypes[OverlayPtr], FrameBuffer))
			{
				if(Overlay->hw_overlay == 1) break;
			}

			SDL_FreeYUVOverlay(Overlay);
			Overlay = NULL;
			OverlayPtr++;
		}

		if(Overlay)
		{
			switch(Overlay->format)
			{
				case SDL_YVYU_OVERLAY: YOffs1 = 0; YOffs2 = 2; VOffs = 1; UOffs = 3; break;
				case SDL_YUY2_OVERLAY: YOffs1 = 0; YOffs2 = 2; VOffs = 3; UOffs = 1; break;
				case SDL_UYVY_OVERLAY: YOffs1 = 1; YOffs2 = 3; VOffs = 2; UOffs = 0; break;
			}

			TableEntryLength = 16;
			TableShift = 3;

			((Uint8 *)&Black)[YOffs1] = 16;
			((Uint8 *)&Black)[YOffs1^1] = 128;

			Black |= (Black << 16);

			DisplayTables = new Uint32[2048];
			MultiDisplayTables = new Uint8[2048];

			/* calculate YUV table */
			int c = 8;
			while(c--)
			{
				double Y, Cr, Cb;

				Y = 16 + ((c&4) ? 65.481 : 0) + ((c&2) ? 128.553 : 0) + ((c&1) ? 24.966 : 0);
				Cb = 128 + ((c&4) ? -37.797 : 0) + ((c&2) ? -74.203 : 0) + ((c&1) ? 112 : 0);
				Cr = 128 + ((c&4) ? 112 : 0) + ((c&2) ? -93.786 : 0) + ((c&1) ? -18.214 : 0);

				if(Y > 255) Y = 255; if(Y < 0) Y = 0;
				if(Cr > 255) Cr = 255; if(Cr < 0) Cr = 0;
				if(Cb > 255) Cb = 255; if(Cb < 0) Cb = 0;

				YUVTable[c^7] = (((int)Y) << 16) | (((int)Cr) << 8) | ((int)Cb);
			}
		}
	}

	if(!Overlay)
	{
		int Attempts = 2;

		while(Attempts--)
		{
			if(TryForMode((DispFlags&SDL_FULLSCREEN) ? 800 : 640, (DispFlags&SDL_FULLSCREEN) ? 600 : 512, (DispFlags&SDL_FULLSCREEN) ? 8 : (NativeBytesPerPixel << 3), DispFlags | ((DispFlags&SDL_FULLSCREEN) ? 0 : SDL_ANYFORMAT)))
			{

				XOffset = (FrameBuffer->w >> 1) - 320;
				YOffset = (FrameBuffer->h >> 1) - 256;

				/* copy 8 target pixels (i.e. 640/80) at a time */
				TableEntryLength = FrameBuffer->format->BytesPerPixel*8;

				switch(FrameBuffer->format->BytesPerPixel)
				{
					case 1 : TableShift = 2; Black = 7; break;
					case 2 : TableShift = 3; break;
					case 3 :
					case 4 : TableShift = 4; break;
				}

				DisplayTables = new Uint32[256 << TableShift];
				MultiDisplayTables = new Uint8[256 << TableShift];

				/* setup palette if applicable */
				if(FrameBuffer->format->BytesPerPixel == 1)
				{
					SDL_Color Pal[8];

					int c = 8;
					while(c--)
					{
						Pal[c^7].r = (c&4) ? 0xff : 0;
						Pal[c^7].g = (c&2) ? 0xff : 0;
						Pal[c^7].b = (c&1) ? 0xff : 0;
					}

					SDL_SetColors(FrameBuffer, Pal, 16, 8);
				}

				Black = SDL_MapRGB(FrameBuffer->format, 0, 0, 0);
			}

			if(!FrameBuffer)
			{
				DispFlags ^= SDL_FULLSCREEN;
			}
			else
			{
				break;
			}
		}
	}
	

	if(FrameBuffer)
	{
		ScaleTarget.y = 0;
		ScaleTarget.h = FrameBuffer->h;

		ScaleTarget.w = CORRECT_W(FrameBuffer->w);
		ScaleTarget.x = (FrameBuffer->w/2) - (ScaleTarget.w/2);

		Surface = true;
		if ( DispFlags & SDL_FULLSCREEN )
			SDL_ShowCursor(SDL_DISABLE);
		else
			SDL_ShowCursor(SDL_ENABLE);

		Iconify(false);
		RecalculatePalette();
		if(DisplayMultiplexed)
			SetMultiplexedPalette(SrcPaletteRGB);
		else
			RefillGraphicsTables();
	}
	else
		PPPtr->DebugMessage(PPDEBUG_SCREENFAILED);

//	if(Restricted)
//		RestrictDisplay(ScaleTarget);
//	else
//		EndRestriction();
}

void CDisplay::FreeSurface()
{
	if(Overlay) { SDL_FreeYUVOverlay(Overlay); Overlay = NULL; }
	delete[] DisplayTables; DisplayTables = NULL;
	delete[] MultiDisplayTables; MultiDisplayTables = NULL;
	Surface = false;
}

bool CDisplay::QueryFullScreen()
{
	return (DispFlags & SDL_FULLSCREEN) ? true : false;
}

void CDisplay::ToggleFullScreen()
{
	FreeSurface();
	DispFlags ^= SDL_FULLSCREEN;
	GetSurface();
}

bool CDisplay::IOCtl(Uint32 Control, void *Parameter, Uint32 TimeStamp)
{
	switch(Control)
	{
//		case IOCTL_PAUSE: FreeSurface(); 	return true;
//		case IOCTL_UNPAUSE: GetSurface();	return true;

		case DISPIOCTL_SEEDPALETTE:
		return SDL_SetColors(FrameBuffer, (SDL_Color *)Parameter, 0, TimeStamp) ? true : false;

		case DISPIOCTL_GETDBUF:
			if(Parameter)
			{
				/* get new frame buffer with page flipping */
				FreeSurface();
				if(TryForMode(FrameBuffer->w, FrameBuffer->h, (DispFlags&SDL_FULLSCREEN) ? 16 : 0, DispFlags | SDL_DOUBLEBUF))
				{
//					printf("getdbuf success!\n");
					return true;
				}
				else
				{
//					printf("getdbuf failure!\n");
					return false;
				}
			}
			else
			{
				/* return to non-flipping mode */
				GetSurface();
				return true;
			}
		break;

		case DISPIOCTL_FLIP:
			SDL_Flip(FrameBuffer);
		return true;

		case DISPIOCTL_GETSCREEN:
			NonAffectingDraw((SDL_Surface *)Parameter);
		return true;
	}

	return CComponentBase::IOCtl(Control, Parameter, TimeStamp);
}


void CDisplay::AttachTo(CProcessPool &pool, Uint32 id)
{
	CComponentBase::AttachTo(pool, id);

	pool.ClaimTrapAddress(id, 0xfe02, 0xff0f);
	pool.ClaimTrapAddress(id, 0xfe03, 0xff0f);
	pool.ClaimTrapAddress(id, 0xfe08, 0xff0f);
	pool.ClaimTrapAddress(id, 0xfe09, 0xff0f);
	pool.ClaimTrapAddress(id, 0xfe0a, 0xff0f);
	pool.ClaimTrapAddress(id, 0xfe0b, 0xff0f);
	pool.ClaimTrapAddress(id, 0xfe0c, 0xff0f);
	pool.ClaimTrapAddress(id, 0xfe0d, 0xff0f);
	pool.ClaimTrapAddress(id, 0xfe0e, 0xff0f);
	pool.ClaimTrapAddress(id, 0xfe0f, 0xff0f);

	FrameStart = CurrentTime = 0;
	VideoWritePtr = VideoReadPtr = 0;
	ULA = (CULA *)PPPtr->GetWellDefinedComponent(COMPONENT_ULA);
	IScanline = 312 << 7;

//	printf("Gathersource: %d\nGathertarget: %d\n", (int)AddrSource, (int)VideoBuffer32); fflush(stdout);
	((C6502 *)PPPtr->GetWellDefinedComponent(COMPONENT_CPU))->SetGathering(AddrSource, VideoBuffer32);
	StartAddr = BackupStartAddr = FrameStartAddr = 0;

	memset(AddrSource, 0, sizeof(Uint16)*40064);
	memset(VideoOffsets8, 0, sizeof(Uint16)*40064);

	GetSurface();
}

/*overall timing:
- signal end of display
- wait 7144 cycles, start drawing first scanline of pixels
- wait 12678 cycles, signal real time clock
- wait 20114 cycles, signal end of display
- wait 7144 cycles, start drawing first scanline of pixels
- wait 12736 cycles, signal real time clock
- wait 20160 cycles, start again

i.e. 39936 frame, 40064 frame, etc*/

void CDisplay::FillAddressTable(Uint8 Mode, int index)
{
	int startx, starty;
	Uint16 Addr, LineAddr;

	/* determine start address if not at top of display - FIX ME */
	if(index > DISPLAY_START)
	{
		index -= DISPLAY_START;
		startx = index&127;
		starty = index >> 7;

		/* currently in a blank line? If so, don't change until next */
		if(EmptyLine[starty])
		{
			starty++;
			startx = 0;
			index = (starty+56) << 7;
		}

		/* seed addresses, descrambling */
		Addr = VideoOffsets8[(starty << 7) + DISPLAY_START];
		LineAddr = VideoOffsets8[index];

		/* switching from a normal display mode to one with black lines? */
		Uint32 ydiff = starty - LineBase;
		if( ((Mode == 3) || (Mode == 6)) != ((AddrMode == 3) || (AddrMode == 6)) )
			LineBase += ((Mode == 3) || (Mode == 6)) ? (ydiff&~7) : (ydiff - ydiff%10);
	}
	else
	{
		LineAddr = Addr = FrameStartAddr;
		startx = 0;
		starty = 0;
		LineBase = 0;
	}

	AddrMode = Mode;
	int BaseIndex;
	BaseIndex = (starty << 7) + DISPLAY_START;

	/* EVOLVE ADDRESS FROM CURRENT POINT TO END OF FRAME BUFFER */
	switch(Mode)
	{
		case 6 :{
			if(!Addr) Addr = 0x6000;
			if(!LineAddr) LineAddr = 0x6000;
			startx >>= 1;

			for(int y = starty; y < 256; y++)
			{
				if((((y-LineBase)%10) > 7) || (y > 249))
				{
					EmptyLine[y] = true;
					BaseIndex += 128;
				}
				else
				{
					EmptyLine[y] = false;

					for(int x = (y == starty) ? startx : 0; x < 40; x++)
					{
						if(LineAddr >= 32768) LineAddr -= 0x2000;
						AddrSource[BaseIndex + 1 + x*2] = AddrSource[BaseIndex + x*2] = DisplayMultiplexed ? Transpose6502Address32(LineAddr) : Transpose6502Address8(LineAddr);
						VideoOffsets8[BaseIndex + x*2] = LineAddr;
						LineAddr += 8;
					}

					if(((y-LineBase)%10) == 7)
						Addr += 313;
					else
						Addr++;

					LineAddr = Addr;
					BaseIndex += 128;
				}
			}
		} break;

		case 3 :{
			if(!Addr) Addr = 0x4000;
			if(!LineAddr) LineAddr = 0x4000;

			for(int y = starty; y < 256; y++)
			{
				EmptyLine[y] = false;
				if((((y-LineBase)%10) > 7) || (y > 249))
					EmptyLine[y] = true;

				for(int x = (y == starty) ? startx : 0; x < 80; x++)
				{
					if(LineAddr >= 32768) LineAddr -= 0x4000;
					AddrSource[BaseIndex + x] = DisplayMultiplexed ? Transpose6502Address32(LineAddr) : Transpose6502Address8(LineAddr);
					VideoOffsets8[BaseIndex + x] = LineAddr;
					LineAddr += 8;
				}

				if(((y-LineBase)%10) == 9)
					Addr += 631;
				else
					Addr++;

				LineAddr = Addr;
				BaseIndex += 128;
			}
		} break;

		case 4 :
		case 5 : {
			if(!Addr) Addr = 0x5800;
			if(!LineAddr) LineAddr = 0x5800;

			startx >>= 1;
			for(int y = starty; y < 256; y++)
			{
				EmptyLine[y] = false;
				for(int x = (y == starty) ? startx : 0; x < 40; x++)
				{
					if(LineAddr >= 32768) LineAddr -= 0x2800;
					AddrSource[BaseIndex + 1 + x*2] = AddrSource[BaseIndex + x*2] = DisplayMultiplexed ? Transpose6502Address32(LineAddr) : Transpose6502Address8(LineAddr);
					VideoOffsets8[BaseIndex + x*2] = LineAddr;
					LineAddr += 8;
				}

				if(((y-LineBase)&7) == 7)
					Addr += 313;
				else
					Addr++;

				BaseIndex += 128;
				LineAddr = Addr;
			}
		} break;

		case 0 :
		case 1 :
		case 2 : {
			if(!Addr) Addr = 0x3000;
			if(!LineAddr) LineAddr = 0x3000;

			for(int y = starty; y < 256; y++)
			{
				EmptyLine[y] = false;

				for(int x = (y == starty) ? startx : 0; x < 80; x++)
				{
					if(LineAddr >= 32768) LineAddr -= 0x5000;
					AddrSource[BaseIndex + x] = DisplayMultiplexed ? Transpose6502Address32(LineAddr) : Transpose6502Address8(LineAddr);
					VideoOffsets8[BaseIndex + x] = LineAddr;
					LineAddr += 8;
				}

				if(((y-LineBase)&7) == 7)
					Addr += 633;
				else
					Addr++;

				BaseIndex += 128;
				LineAddr = Addr;
			}
		} break;
	}
	fflush(stdout);
}

void CDisplay::SetMode(Uint32 TimeStamp, Uint8 Mode)
{
//	printf("%d at offset %d:%d\n", Mode, (TimeStamp - FrameStart) >> 7, (TimeStamp - FrameStart) &127);
	if(Mode != MostRecentMode)
	{
		VideoEventQueue[VideoWritePtr].Type = VideoEvent::MODE;
		VideoEventQueue[VideoWritePtr].TimeStamp = TimeStamp;
		VideoEventQueue[VideoWritePtr].Value = Mode;
		VideoWritePtr = (VideoWritePtr+1)&(CDISPLAY_VIDEOEVENT_LENGTH-1);

		int NewClass, OldClass;

		switch(Mode)
		{
			default:
			case 0 :
			case 1 : 
			case 2 : NewClass = 0; break;
			case 3 : NewClass = 1; break;
			case 4 :
			case 5 : NewClass = 2; break;
			case 6 : NewClass = 3; break;
		}
		switch(MostRecentMode)
		{
			default:
			case 0 :
			case 1 : 
			case 2 : OldClass = 0; break;
			case 3 : OldClass = 1; break;
			case 4 :
			case 5 : OldClass = 2; break;
			case 6 : OldClass = 3; break;
		}

		if(NewClass^OldClass)
			FillAddressTable(Mode, TimeStamp - FrameStart);		
		switch(Mode)
		{
			default:					ULA->SetULARAMTiming(HALTING_NONE);		break;
			case 0: case 1: case 2:		ULA->SetULARAMTiming(HALTING_NORMAL);	break;
			case 3:						ULA->SetULARAMTiming(HALTING_MODE3);	break;
		}

		MostRecentMode = Mode;
	}
}

void CDisplay::SumEvents()
{
	if(TablesDirty)
	{
		RefillGraphicsTables();
		TablesDirty = false;
	}
}

/*Uint8 MyMapRGB(SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b)
{
	int c = 256;
	unsigned int low = ~0;
	Uint8 rs;

	if(!r && !g && !b)
		c = 24;
	while(c--)
	{
		int rdiff, gdiff, bdiff;

		rdiff = r - fmt->palette->colors[c].r;
		gdiff = g - fmt->palette->colors[c].g;
		bdiff = b - fmt->palette->colors[c].b;

		unsigned int nd = (rdiff*rdiff) + (gdiff*gdiff) + (bdiff*bdiff);
		if(nd < low)
		{
			rs = c;
			low = nd;
		}
	}

	return rs;
}*/

#define MapRGB(r, g, b) Overlay ? YUVTable[r|g|b] : SDL_MapRGB( FrameBuffer->format, r?0:0xff, g?0:0xff, b?0:0xff);

void CDisplay::EnactEvent()
{
	switch(VideoEventQueue[VideoReadPtr].Type)
	{
		case VideoEvent::MODE:
			ModeChanged = true;
			CMode = VideoEventQueue[VideoReadPtr].Value;
			ByteMask = (CMode > 3) ? (~1) : (~0);

			TablesDirty = true;
		break;

		case VideoEvent::PALETTE :
			if(PaletteBytes[VideoEventQueue[VideoReadPtr].Address] != VideoEventQueue[VideoReadPtr].Value)
			{
				PaletteBytes[VideoEventQueue[VideoReadPtr].Address] = VideoEventQueue[VideoReadPtr].Value;
				if(CMode == 2) TablesDirty = true;

				switch(VideoEventQueue[VideoReadPtr].Address)
				{
					case 0:
					case 1:

						PaletteColours[0] =	MapRGB(
										((PaletteBytes[0x01]&0x01) << 2),
										((PaletteBytes[0x01]&0x10) >> 3),
										((PaletteBytes[0x00]&0x10) >> 4)
										);

						PaletteColours[2] =	MapRGB(
										((PaletteBytes[0x01]&0x02) << 1),
										((PaletteBytes[0x01]&0x20) >> 4),
										((PaletteBytes[0x00]&0x20) >> 5)
										);

						PaletteColours[8] =	MapRGB(
										((PaletteBytes[0x01]&0x04) << 0),
										((PaletteBytes[0x00]&0x04) >> 1),
										((PaletteBytes[0x00]&0x40) >> 6)
										);

						PaletteColours[10] = MapRGB(
										((PaletteBytes[0x01]&0x08) >> 1),
										((PaletteBytes[0x00]&0x08) >> 2),
										((PaletteBytes[0x00]&0x80) >> 7)
										);

					TablesDirty = true;
					break;

					case 2 :
					case 3 :
						PaletteColours[4] =	MapRGB(
										((PaletteBytes[0x03]&0x01) << 2),
										((PaletteBytes[0x03]&0x10) >> 3),
										((PaletteBytes[0x02]&0x10) >> 4)
										);

						PaletteColours[6] =	MapRGB(
										((PaletteBytes[0x03]&0x02) << 1),
										((PaletteBytes[0x03]&0x20) >> 4),
										((PaletteBytes[0x02]&0x20) >> 5)
										);

						PaletteColours[12] = MapRGB(
										((PaletteBytes[0x03]&0x04) << 0),
										((PaletteBytes[0x02]&0x04) >> 1),
										((PaletteBytes[0x02]&0x40) >> 6)
										);

						PaletteColours[14] = MapRGB(
										((PaletteBytes[0x03]&0x08) >> 1),
										((PaletteBytes[0x02]&0x08) >> 2),
										((PaletteBytes[0x02]&0x80) >> 7)
										);
					break;

					case 4 :
					case 5 :
						PaletteColours[5] =	MapRGB(
										((PaletteBytes[0x05]&0x01) << 2),
										((PaletteBytes[0x05]&0x10) >> 3),
										((PaletteBytes[0x04]&0x10) >> 4)
										);

						PaletteColours[7] =	MapRGB(
										((PaletteBytes[0x05]&0x02) << 1),
										((PaletteBytes[0x05]&0x20) >> 4),
										((PaletteBytes[0x04]&0x20) >> 5)
										);

						PaletteColours[13] = MapRGB(
										((PaletteBytes[0x05]&0x04) << 0),
										((PaletteBytes[0x04]&0x04) >> 1),
										((PaletteBytes[0x04]&0x40) >> 6)
										);

						PaletteColours[15] = MapRGB(
										((PaletteBytes[0x05]&0x08) >> 1),
										((PaletteBytes[0x04]&0x08) >> 2),
										((PaletteBytes[0x04]&0x80) >> 7)
										);
					break;

					case 6 :
					case 7 :
						PaletteColours[1] =	MapRGB(
										((PaletteBytes[0x07]&0x01) << 2),
										((PaletteBytes[0x07]&0x10) >> 3),
										((PaletteBytes[0x06]&0x10) >> 4)
										);

						PaletteColours[3] =	MapRGB(
										((PaletteBytes[0x07]&0x02) << 1),
										((PaletteBytes[0x07]&0x20) >> 4),
										((PaletteBytes[0x06]&0x20) >> 5)
										);

						PaletteColours[9] =	MapRGB(
										((PaletteBytes[0x07]&0x04) << 0),
										((PaletteBytes[0x06]&0x04) >> 1),
										((PaletteBytes[0x06]&0x40) >> 6)
										);

						PaletteColours[11] = MapRGB(
										((PaletteBytes[0x07]&0x08) >> 1),
										((PaletteBytes[0x06]&0x08) >> 2),
										((PaletteBytes[0x06]&0x80) >> 7)
										);
					break;
				}

//				if(DisplayMultiplexed) TablesDirty = false;
			}
		break;
	}

	VideoReadPtr = (VideoReadPtr+1)&(CDISPLAY_VIDEOEVENT_LENGTH-1);
}

Uint8 CDisplay::Read(Uint16 Addr)
{
	switch(Addr&0xf)
	{
		default: return 0;
		case 0x02: return (StartAddr&0x01c0) >> 1;
		case 0x03: return (StartAddr&0x7e00) >> 9;
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
				/* this isn't quite right! Some may still be on the event queue */
		return PaletteBytes[(Addr&0xf) - 8];
	}
}

bool CDisplay::Write(Uint16 Addr, Uint32 TimeStamp, Uint8 Data8, Uint32 Data32)
{
	switch(Addr&0xf)
	{
		/* screen start address */
		case 0x02 :
				StartAddr = (StartAddr&0x7e00) | ((Data8<<1)&0x01c0); 

				if((Uint32)(TimeStamp - FrameStart) < DISPLAY_START)
				{
					FrameStartAddr = StartAddr;

					if(FrameStartAddr^BackupStartAddr)
						FillAddressTable(CMode, 0);
				}
			break;
		case 0x03 :
				StartAddr = (StartAddr&0x01c0) | ((Data8<<9)&0x7e00);
				if((Uint32)(TimeStamp - FrameStart) < DISPLAY_START)
				{
					FrameStartAddr = StartAddr;

					if(FrameStartAddr^BackupStartAddr)
						FillAddressTable(CMode, 0);
				}
			break;

		/* palette */
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
		{
//			static Uint32 OTimeStamp = 0;
//			printf("%d\n", (Uint32)(TimeStamp - OTimeStamp)); OTimeStamp = TimeStamp;
			Addr &= 0xf;

			VideoEventQueue[VideoWritePtr].Type = VideoEvent::PALETTE;
			VideoEventQueue[VideoWritePtr].TimeStamp = TimeStamp;
			VideoEventQueue[VideoWritePtr].Address = Addr - 8;
			VideoEventQueue[VideoWritePtr].Value = Data8;
			VideoWritePtr = (VideoWritePtr+1)&(CDISPLAY_VIDEOEVENT_LENGTH-1);
		}
		break;
	}

	return false;
}
