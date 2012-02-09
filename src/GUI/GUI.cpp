#if !defined(USE_NATIVE_GUI) && !defined(NO_GUI)

/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	GUI.cpp
	=======

	This will eventually be the launching point for the GUI. At the minute
	it's nothing really.

	code for opening a URL:
ShellExecute (GetSafeHwnd(),
                       "open",
                       csURL,
                       NULL,
                       NULL,
                       SW_SHOWNORMAL);

*/
#include "GUI.h"
#include "../ULA.h"
#include "../Display.h"
#include <stdlib.h>
#include "GUIObjects.h"
#include "MainScreen.h"
#include "GFXDefines.h"
#include "../HostMachine/HostMachine.h"

SDL_Surface *OptimiseSurface(SDL_Surface *input, bool alpha)
{
	if(input->format->Aloss == 8)
		alpha = false;

	SDL_Surface *r = NULL;
	if(alpha)
	{
		if(SDL_GetVideoInfo()->blit_hw_A)
		{
			SDL_Surface *T = SDL_CreateRGBSurface(0, 1, 1, 32, 0xff000000, 0xff0000, 0xff00, 0xff);
			SDL_Surface *T2 = SDL_DisplayFormatAlpha(T);
			SDL_FreeSurface(T);

			r = SDL_ConvertSurface(input, T2->format, SDL_HWSURFACE);
			if(!r) r = SDL_ConvertSurface(input, T2->format, 0);
			SDL_FreeSurface(T2);
		}
		else
			r = SDL_DisplayFormatAlpha(input);
	}
	else
	{
		r = SDL_ConvertSurface(input, SDL_GetVideoInfo()->vfmt, SDL_HWSURFACE);
		if(!r) r = SDL_ConvertSurface(input, SDL_GetVideoInfo()->vfmt, 0);
		SDL_SetAlpha(r, 0, SDL_ALPHA_OPAQUE);
	}

	return r;
}

SDL_Surface *OptimalSurface(int w, int h, bool alpha)
{
	/* try to get an hwsurface */
	SDL_PixelFormat *fmt = SDL_GetVideoInfo()->vfmt;

	SDL_Surface *Temp = NULL;
	if(SDL_GetVideoInfo()->blit_hw_A || !alpha) SDL_CreateRGBSurface(SDL_HWSURFACE, w, h, 32, 0xff000000, 0xff0000, 0xff00, 0xff);
	if(!Temp) Temp = SDL_CreateRGBSurface(0, w, h, 32, 0xff000000, 0xff0000, 0xff00, 0xff);
	if(!Temp) return NULL;

	SDL_FillRect(Temp, NULL, SDL_MapRGBA(Temp->format, 0, 0, 0, 0));
	SDL_Surface *R = OptimiseSurface(Temp, alpha);
	if(R)
	{
		SDL_FreeSurface(Temp);
		return R;
	}

	return Temp;
}

/* temporary measure! */
void CGUI::InstallResource(int id, char *i, char *m)
{
	char *img = GetHost()->ResolveFileName(i);
	char *msk = GetHost()->ResolveFileName(m);
	CUEFChunkSelector *GFXFile = GetHost()->GetUEFSelector("%GFXPATH%/guidata.uef", 0x0000);

	SDL_Surface *Image = SDL_LoadBMP(img);
	if(!Image) { delete[] img; delete[] msk; return; }
	SDL_Surface *Mask = msk ? SDL_LoadBMP(msk) : NULL;
	if(Mask)
	{
		/* okay, build composite bitmap of gfx data and alpha channel */

		int rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		rmask = 0xff000000;
	    gmask = 0x00ff0000;
		bmask = 0x0000ff00;
	    amask = 0x000000ff;
#else
	    rmask = 0x000000ff;
	    gmask = 0x0000ff00;
	    bmask = 0x00ff0000;
	    amask = 0xff000000;
#endif

		SDL_Surface *Composite = SDL_CreateRGBSurface(SDL_SRCALPHA, Image->w, Image->h, 32, rmask, gmask, bmask, amask);

		SDL_Surface *T = SDL_ConvertSurface(Image, Composite->format, 0);
		SDL_FreeSurface(Image); Image = T; 

		SDL_LockSurface(Composite);
		SDL_LockSurface(Image);
		SDL_LockSurface(Mask);

		for(int y = 0; y < Image->h; y++)
		{
			for(int x = 0; x < Image->w; x++)
			{
				int Component = 3;
				while(Component--)
					((Uint8 *)Composite->pixels)[y*Composite->pitch + (x << 2) + Component] = 
					((Uint8 *)Image->pixels)[y*Image->pitch + (x << 2) + Component];

				((Uint8 *)Composite->pixels)[y*Composite->pitch + (x << 2) + 3] = ((Uint8 *)Mask->pixels)[y*Mask->pitch + x];
			}
		}

		SDL_UnlockSurface(Composite);
		SDL_UnlockSurface(Image);
		SDL_UnlockSurface(Mask);

		SDL_FreeSurface(Mask);
		SDL_FreeSurface(Image);

		SrcResources[id] = Composite;
	}
	else
		SrcResources[id] = Image;

	CUEFChunk *NewChunk = GFXFile->EstablishChunk();
	NewChunk->SetId(0xff01 + id);

	/* type: paletted, 24bpp or 32bpp */
	switch(SrcResources[id]->format->BytesPerPixel)
	{
		case 1:	NewChunk->PutC(0); break;
		default:NewChunk->PutC(1); break;
		case 4:	NewChunk->PutC(2); break;
	}

	/* dimensions */
	NewChunk->Put16(SrcResources[id]->w);
	NewChunk->Put16(SrcResources[id]->h);

	/* image data */
	SDL_LockSurface(SrcResources[id]);
	switch(SrcResources[id]->format->BytesPerPixel)
	{
		case 1:
		{
			Uint8 *PelPtr = (Uint8 *)SrcResources[id]->pixels;
			for(int y = 0; y < SrcResources[id]->h; y++)
			{
				NewChunk->Write(PelPtr, SrcResources[id]->w);
				PelPtr += SrcResources[id]->pitch;
			}
		}
		break;

		case 3:
		{
			for(int y = 0; y < SrcResources[id]->h; y++)
			{
				for(int x = 0; x < SrcResources[id]->w; x++)
				{
					Uint8 R, G, B;
					SDL_GetRGB(*((Uint32 *)&((Uint8 *)SrcResources[id]->pixels)[ (SrcResources[id]->pitch*y) + x*SrcResources[id]->format->BytesPerPixel]), SrcResources[id]->format, &R, &G, &B);

					NewChunk->PutC(R);
					NewChunk->PutC(G);
					NewChunk->PutC(B);
				}
			}
		}
		break;

		case 4:
		{
			for(int y = 0; y < SrcResources[id]->h; y++)
			{
				for(int x = 0; x < SrcResources[id]->w; x++)
				{
					Uint8 R, G, B, A;
					SDL_GetRGBA(*((Uint32 *)&((Uint8 *)SrcResources[id]->pixels)[ (SrcResources[id]->pitch*y) + x*SrcResources[id]->format->BytesPerPixel]), SrcResources[id]->format, &R, &G, &B, &A);

					NewChunk->PutC(R);
					NewChunk->PutC(G);
					NewChunk->PutC(B);
					NewChunk->PutC(A);
				}
			}
		}
		break;
	}
	SDL_UnlockSurface(SrcResources[id]);

	/* palette maybe */
	if(SrcResources[id]->format->BytesPerPixel == 1)
	{
		for(int c = 0; c < 256; c++)
		{
			NewChunk->PutC(SrcResources[id]->format->palette->colors[c].r);
			NewChunk->PutC(SrcResources[id]->format->palette->colors[c].g);
			NewChunk->PutC(SrcResources[id]->format->palette->colors[c].b);
		}
	}

	GetHost()->ReleaseUEFSelector(GFXFile);
	delete[] img;
	delete[] msk;
}

bool CGUI::GetImage(CUEFChunkSelector *sel, int id)
{
	if(!sel)
		return false;
	if(!sel->FindId(id+0xff01))
		return false;

	/* type: paletted, 24bpp or 32bpp */
	int BytesPerPixel;
	switch(sel->CurrentChunk()->GetC())
	{
		case 0: BytesPerPixel = 1; break;
		case 1: BytesPerPixel = 3; break;
		case 2: BytesPerPixel = 4; break;
		default: return false;
	}

	int W, H;
	W = sel->CurrentChunk()->Get16();
	H = sel->CurrentChunk()->Get16();

	switch(BytesPerPixel)
	{
		case 1:
			SrcResources[id] = SDL_CreateRGBSurface(0, W, H, 8, 0, 0, 0, 0);
		break;

		case 3:
#ifdef SDL_LIL_ENDIAN
			SrcResources[id] = SDL_CreateRGBSurface(0, W, H, 24, 0xff, 0xff00, 0xff0000, 0);
#else
			SrcResources[id] = SDL_CreateRGBSurface(0, W, H, 24, 0xff0000, 0xff00, 0xff, 0);
#endif
		break;

		case 4:
			SrcResources[id] = SDL_CreateRGBSurface(0, W, H, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
		break;
	}

	SDL_LockSurface(SrcResources[id]);
	switch(BytesPerPixel)
	{
		case 1:
		{
			Uint8 *PelPtr = (Uint8 *)SrcResources[id]->pixels;
			for(int y = 0; y < H; y++)
			{
				sel->CurrentChunk()->Read(PelPtr, SrcResources[id]->w);
				PelPtr += SrcResources[id]->pitch;
			}
		}
		break;

		case 3:
		{
			for(int y = 0; y < SrcResources[id]->h; y++)
			{
				Uint8 *Base = &((Uint8 *)SrcResources[id]->pixels)[ (SrcResources[id]->pitch*y)];
				for(int x = 0; x < SrcResources[id]->w; x++)
				{
					Base[0] = sel->CurrentChunk()->GetC();
					Base[1] = sel->CurrentChunk()->GetC();
					Base[2] = sel->CurrentChunk()->GetC();
					Base += 3;
				}
			}
		}
		break;

		case 4:
		{
			for(int y = 0; y < SrcResources[id]->h; y++)
			{
				for(int x = 0; x < SrcResources[id]->w; x++)
				{
					Uint8 R, G, B, A;
					R = sel->CurrentChunk()->GetC();
					G = sel->CurrentChunk()->GetC();
					B = sel->CurrentChunk()->GetC();
					A = sel->CurrentChunk()->GetC();

					Uint32 TotalColour = SDL_MapRGBA(SrcResources[id]->format, R, G, B, A);
					*((Uint32 *)&((Uint8 *)SrcResources[id]->pixels)[ (SrcResources[id]->pitch*y) + x*SrcResources[id]->format->BytesPerPixel]) = TotalColour;
				}
			}
		}
		break;
	}
	SDL_UnlockSurface(SrcResources[id]);

	/* read palette? */
	if(BytesPerPixel == 1)
	{
		SDL_Color Palette[256];
		for(int c =0; c < 256; c++)
		{
			Palette[c].r = sel->CurrentChunk()->GetC();
			Palette[c].g = sel->CurrentChunk()->GetC();
			Palette[c].b = sel->CurrentChunk()->GetC();
		}

		SDL_SetColors(SrcResources[id], Palette, 0, 256);
	}

	return true;
}

void CGUI::MapResources()
{
	SDL_PixelFormat *SInfo = SDL_GetVideoSurface()->format;

	/* if paletted, install palette */
	if(SInfo->BytesPerPixel == 1)
	{
		FILE *pal = fopen("GFX/gui.pal", "rt");

			/* ignore first two lines */
			char TBuf[256];
			fscanf(pal, "%s\n", TBuf);
			fscanf(pal, "%s\n", TBuf);

			/* get number of palette entries */
			int NumEntries;
			SDL_Color ColSet[256];
			fscanf(pal, "%d\n", &NumEntries);

			if(NumEntries > 256) NumEntries = 256;

			int c;
			for(c = 0; c < NumEntries; c++)
				fscanf(pal, "%d %d %d\n", &ColSet[c].r, &ColSet[c].g, &ColSet[c].b);
			for(;c < 256; c++)
				ColSet[c].r = ColSet[c].g = ColSet[c].b = 0;

		fclose(pal);

		((CDisplay *)PPool->GetWellDefinedComponent(COMPONENT_DISPLAY))->IOCtl(DISPIOCTL_SEEDPALETTE, ColSet, NumEntries);
//		c = SDL_SetColors(DisplayBuffer, ColSet, 256 - NumEntries, NumEntries);
		c = SDL_SetColors(StaticBuffer, ColSet, 256 - NumEntries, NumEntries);
	}

	int c= 256;
	while(c--)
	{
		if(SrcResources[c])
		{
			if(SrcResources[c]->format->Aloss == 8)
				Resources[c] = OptimiseSurface(SrcResources[c]);
			else
			{
				/* if in paletted mode, just go for a src keyed fade to black */
				if(SInfo->BytesPerPixel == 1)
				{
					Resources[c] = OptimiseSurface(SrcResources[c]);
					SDL_LockSurface(Resources[c]);
					SDL_LockSurface(SrcResources[c]);

					/* find an unused colour */
					bool ColourList[256];
					int cc = 256, x, y;
					while(cc--) ColourList[cc] = false;
					
					for(y = 0; y < Resources[c]->h; y++)
						for(x = 0; x < Resources[c]->w; x++)
							ColourList[((Uint8 *)Resources[c]->pixels)[ y * Resources[c]->pitch + x]] = true;

					Uint8 UnusedColour = 0;
					cc = 256;
					while(cc--)
						if(!ColourList[cc]) {UnusedColour = cc; break; }

					for(y = 0; y < Resources[c]->h; y++)
					{
						for(x = 0; x < Resources[c]->w; x++)
						{
							if( ((Uint8 *)SrcResources[c]->pixels)[ y * SrcResources[c]->pitch + (x << 2) + 3] < 0x80)
								((Uint8 *)Resources[c]->pixels)[ y * Resources[c]->pitch + x] = UnusedColour;

						}
					}

					SDL_UnlockSurface(Resources[c]);
					SDL_UnlockSurface(SrcResources[c]);

					SDL_SetColorKey(Resources[c], SDL_SRCCOLORKEY, UnusedColour);
				}
				else
					Resources[c] = OptimiseSurface(SrcResources[c], true);
			}
		}
	}
}

void CGUI::FreeResources()
{
	int c = 256;
	while(c--)
	{
		if(Resources[c])
		{
			SDL_FreeSurface(Resources[c]);
			Resources[c] = NULL;
		}
	}
}

CProcessPool *CGUI::GetProcessPool()
{
	return PPool;
}

BasicConfigurationStore *CGUI::GetConfigFile()
{
	return ConfigFile;
}

TTF_Font *Safe_TTF_OpenFont(const char *file, int ptsize)
{
	SDL_RWops *FPtr = SDL_RWFromFile(file, "r");
	if(!FPtr) return NULL;
	SDL_FreeRW(FPtr);

	return TTF_OpenFont(file, ptsize);
}

CGUI::CGUI(CProcessPool *p, BasicConfigurationStore*cfg)
{
	PPool = p;
	ConfigFile = cfg;
//	DisplayBuffer = NULL;
	TTF_Init();

	char *Fontname = GetHost()->ResolveFileName("%GFXPATH%/vera.ttf");
	Font12 = Safe_TTF_OpenFont(Fontname, 12);
	Font14 = Safe_TTF_OpenFont(Fontname, 14);
	Font18 = Safe_TTF_OpenFont(Fontname, 18);
	Font24 = Safe_TTF_OpenFont(Fontname, 24);
	delete[] Fontname;

	/* in IE:

			H1 = 24
			H2 = 18
			H3 = 14
			normal = 12
	*/

	int c = 256;
	while(c--)
		SrcResources[c] = Resources[c] = NULL;

	CUEFChunkSelector *GFXFile = GetHost()->GetUEFSelector("%GFXPATH%/guidata.uef", 0x0000);

	if(!GetImage(GFXFile, GFX_CURSOR))
		InstallResource(GFX_CURSOR, "%GFXPATH%/GFX/cursor.bmp", "%GFXPATH%/GFX/cursormask.bmp");
	if(!GetImage(GFXFile, GFX_ELECTRON))
		InstallResource(GFX_ELECTRON, "%GFXPATH%/GFX/transelk.bmp");
	if(!GetImage(GFXFile, GFX_FOLDER))
		InstallResource(GFX_FOLDER, "%GFXPATH%/GFX/folder.bmp", "%GFXPATH%/GFX/foldermask.bmp");
	if(!GetImage(GFXFile, GFX_UPFOLDER))
		InstallResource(GFX_UPFOLDER, "%GFXPATH%/GFX/upfolder.bmp", "%GFXPATH%/GFX/foldermask.bmp");
	if(!GetImage(GFXFile, GFX_TAPE))
		InstallResource(GFX_TAPE, "%GFXPATH%/GFX/tape.bmp", "%GFXPATH%/GFX/tapemask.bmp");
	if(!GetImage(GFXFile, GFX_UNKNOWN))
		InstallResource(GFX_UNKNOWN, "%GFXPATH%/GFX/unknown.bmp", "%GFXPATH%/GFX/unknownmask.bmp");
	if(!GetImage(GFXFile, GFX_SNAPSHOT))
		InstallResource(GFX_SNAPSHOT, "%GFXPATH%/GFX/snapshot.bmp", "%GFXPATH%/GFX/snapshotmask.bmp");
	if(!GetImage(GFXFile, GFX_DISC))
		InstallResource(GFX_DISC, "%GFXPATH%/GFX/disc.bmp", "%GFXPATH%/GFX/discmask.bmp");

	GetHost()->ReleaseUEFSelector(GFXFile);
}

CGUI::~CGUI()
{
	int c = 256;
	while(c--)
	{
		if(Resources[c]) SDL_FreeSurface(Resources[c]);
		if(SrcResources[c]) SDL_FreeSurface(SrcResources[c]);
	}
	if(Font12) TTF_CloseFont(Font12);
	if(Font14) TTF_CloseFont(Font14);
	if(Font18) TTF_CloseFont(Font18);
	if(Font24) TTF_CloseFont(Font24);
	TTF_Quit();
}

/*
	pretty much as SDL_BlitSurface, but blits everywhere except dstrect
*/
void SDL_BlitSurfaceNOT(SDL_Surface *src, SDL_Surface *dst, SDL_Rect *dstrect)
{
	SDL_Rect Target;

	/* left strip */
	Target.x = 0;
	if(Target.w = dstrect->x)
	{
		Target.y = 0;
		Target.h = src->h;
		SDL_BlitSurface(src, &Target, dst, &Target);
	}

	/* right strip */
	Target.x = dstrect->x+dstrect->w;
	if(Target.w = src->w-Target.x)
	{
		Target.y = 0;
		Target.h = src->h;
		SDL_BlitSurface(src, &Target, dst, &Target);
	}

	/* top strip */
	Target.x = dstrect->x;
	Target.w = dstrect->w;
	Target.y = 0;
	if(Target.h = dstrect->y)
		SDL_BlitSurface(src, &Target, dst, &Target);

	/* bottom strip */
	Target.y = dstrect->y+dstrect->h;
	if(Target.h = src->h-Target.y)
		SDL_BlitSurface(src, &Target, dst, &Target);

	SDL_UpdateRect(dst, 0, 0, 0, 0);
}

bool CGUI::Go()
{
	bool Fullscreen;
	/* restrict display */
	CDisplay *Disp = (CDisplay *)PPool->GetWellDefinedComponent(COMPONENT_DISPLAY);
	SDL_Surface *Front;

	/* map resources, installing palette, etc */

	/* get screen dimensions, generate two intermediate buffers */
	int ScrW, ScrH;

	Disp->IOCtl(DISPIOCTL_GETDBUF, this, 0);

	Front = Disp->GetFrontBuffer();
	ScrW = Front->w; ScrH = Front->h;
	Fullscreen = (Front->flags&SDL_FULLSCREEN) ? true : false;
	Disp->ReleaseFrontBuffer();

	StaticBuffer = OptimalSurface(ScrW, ScrH);

	MapResources();

	// prepare display
	SDL_Rect WholeDisplay;
	WholeDisplay.x = WholeDisplay.y = 0;
	WholeDisplay.w = ScrW; WholeDisplay.h = ScrH;
	GUIObject = new CGUIMainDlg(*this, StaticBuffer, WholeDisplay);

	MessagesPending = 0;
	MessageQueue = new CGUIMessage[16];

	int MouseX, MouseY, BState;
	BState = SDL_GetMouseState(&MouseX, &MouseY);

	QMessage = false;
	Quit = false;
	bool HaveMouse = true;
	while(!Quit)
	{
		// refresh buffer bwith all static elements
		Front = Disp->GetFrontBuffer();
		SDL_BlitSurface(StaticBuffer, NULL, Front, NULL);
		
		// get mouse state
		int NewMouseX, NewMouseY, NewBState;
		NewBState = SDL_GetMouseState(&NewMouseX, &NewMouseY);

		// draw display
		CGUIMessage DrawMessage;
		DrawMessage.Type = CGUIMessage::DRAW;
		DrawMessage.PositionX = 0; DrawMessage.PositionY = 0;
		DrawMessage.Data.Draw.Target = Front;
		GUIObject->Message(&DrawMessage);

		// add cursor if necessary
		if(	(SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE) &&
			((SDL_GetAppState()&SDL_APPMOUSEFOCUS) || Fullscreen))
			AddGraphic(Front, GFX_CURSOR, NewMouseX, NewMouseY);

		// communicate mouse movements
		CGUIMessage MouseMessage;
		MouseMessage.PositionX = NewMouseX;
		MouseMessage.PositionY = NewMouseY;
		MouseMessage.Buttons = NewBState;
		MouseMessage.Data.Mouse.DeltaX = NewMouseX - MouseX;
		MouseMessage.Data.Mouse.DeltaY = NewMouseY - MouseY;
		MouseMessage.Type = CGUIMessage::MOUSEOVER;
		GUIObject->Message(&MouseMessage);

		MouseX = NewMouseX; MouseY = NewMouseY;

		// communicate button changes
		NewBState ^= BState; BState ^= NewBState;
		if(NewBState)
		{
			MouseMessage.Type = CGUIMessage::BUTTONS;
			MouseMessage.Data.Button.Changes = NewBState;
			GUIObject->Message(&MouseMessage);
		}

		// blit display, check it hasn't changed
//		SDL_BlitSurface(DisplayBuffer, NULL, Front, NULL);
		SDL_UpdateRect(Front, 0, 0, 0,0);
		Disp->ReleaseFrontBuffer();
		Disp->IOCtl(DISPIOCTL_FLIP, NULL, 0);

		// pump events
		SDL_PumpEvents();

		// post any pending messages
		while(MessagesPending--)
			GUIObject->Message(&MessageQueue[MessagesPending]);
		MessagesPending = 0;

		// get latest interesting keys, etc
		((CULA *)(PPool->GetWellDefinedComponent(COMPONENT_ULA)))->UpdateKeyTable();

		SDL_Event ev;
		while(SDL_PollEvent(&ev))
		switch(ev.type)
		{
			case SDL_QUIT:
				Quit = QMessage = true;
			break;

			// this is the conduit through which information is passed back here from the
			// emulation thread
			case SDL_USEREVENT:
				switch(ev.user.code)
				{
					case PPDEBUG_USERQUIT: QMessage = true;
					case PPDEBUG_GUI: Quit = true; break;
				}
			break;

			case SDL_APPMOUSEFOCUS:
				HaveMouse = ev.active.gain ? true : false;
			break;
		}
	}

	Disp->IOCtl(DISPIOCTL_GETDBUF, NULL, 0);

	delete GUIObject;
	delete[] MessageQueue;
	FreeResources();
//	SDL_FreeSurface(DisplayBuffer);
	SDL_FreeSurface(StaticBuffer);

	return QMessage;
}

void CGUI::PostMessage(CGUIMessage *m)
{
	/* don't post now, simply store for later - prevents nested message calls */
//	GUIObject->Message(m);
	MessageQueue[MessagesPending] = *m;
	MessagesPending++;
}

void CGUI::SignalQuit(bool QuitEmulator)
{
	Quit = true;
	QMessage |= QuitEmulator;
}

void CGUI::AddGraphic(SDL_Surface *s, int GraphicID, int x, int y)
{
	if(Resources[GraphicID])
	{
		SDL_Rect TRect;
		TRect.x = x; TRect.y = y; TRect.w = Resources[GraphicID]->w; TRect.h = Resources[GraphicID]->h;
		SDL_BlitSurface(Resources[GraphicID], NULL, s, &TRect);
	}
	else
	{
		SDL_Rect TRect;
		TRect.x = x; TRect.y = y; TRect.w = 16; TRect.h = 16;
		SDL_FillRect(s, &TRect, SDL_MapRGB(s->format, 0xff, 0xff, 0xff));
	}
}

SDL_Rect CGUI::GetGraphicDimensions(int GraphicID)
{
	SDL_Rect R;

	R.x = R.y = 0;
	if(Resources[GraphicID])
	{
		R.w = Resources[GraphicID]->w;
		R.h = Resources[GraphicID]->h;
	}
	else
		R.w = R.h = 0;

	return R;
}

TTF_Font *CGUI::GetFont(int size)
{
	switch(size)
	{
		case 12: return Font12;
		case 14: return Font14;
		case 18: return Font18;
		case 24: return Font24;
	}

	return NULL;
}
#endif
