#if !defined(USE_NATIVE_GUI) && !defined(NO_GUI)

#include "GUIObjects.h"
#include <string.h>
#include <malloc.h>

/*

	BASE CLASS FOR OBJECTS

*/
void CGUIObjectBase::Message(CGUIMessage *M)
{
	if(M->Type == CGUIMessage::DRAW && DisplaySurface)
	{
		SDL_Rect T = Target;

		T.x += M->PositionX; T.y += M->PositionY;

		/* don't accumulate alpha */
		/*bool WasAlpha = false;
		if(M->Data.Draw.Target->format->Amask && DisplaySurface->format->Amask)
		{
			WasAlpha = (DisplaySurface->flags&SDL_SRCALPHA) ? true : false;
			SDL_SetAlpha(DisplaySurface, 0, DisplaySurface->format->alpha);
		}*/
		SDL_BlitSurface(DisplaySurface, NULL, M->Data.Draw.Target, &T);
		/*if(WasAlpha)
			SDL_SetAlpha(DisplaySurface, SDL_SRCALPHA, DisplaySurface->format->alpha);*/
	}
}
SDL_Rect CGUIObjectBase::GetArea() { return Target; }
CGUIObjectBase::~CGUIObjectBase()
{
	if(DisplaySurface) SDL_FreeSurface(DisplaySurface);
}
void CGUIObjectBase::Move(int OfsX, int OfsY)
{
	Target.x += OfsX; Target.y += OfsY;
}

void CGUIObjectBase::MakeStatic(SDL_Surface *Static)
{
	SDL_BlitSurface(DisplaySurface, NULL, Static, &Target);
	SDL_FreeSurface(DisplaySurface); DisplaySurface = NULL;
}
void CGUIObjectBase::ChangeColour(SDL_Color &NewColour) {}


/*

	BASE CLASS FOR COLLECTIONS

*/
void CGUIObjectCollection::Message(CGUIMessage *Msg)
{
	CGUIObjectBase::Message(Msg);

	/* set clipping rectangle and allow for further adjustment if this is a draw */
	SDL_Rect OldClipper;
	if(Msg->Type == CGUIMessage::DRAW)
	{
		SDL_GetClipRect(Msg->Data.Draw.Target, &OldClipper);
		SDL_Rect NewRect = SubArea;

		NewRect.x += Msg->PositionX;
		NewRect.y += Msg->PositionY;

		/* separating planes */
		if(NewRect.x > OldClipper.x+OldClipper.w) return;
		if(NewRect.y > OldClipper.y+OldClipper.h) return;
		if(NewRect.x+NewRect.w < OldClipper.x) return;
		if(NewRect.y+NewRect.h < OldClipper.y) return;

		/* adjust clip box */
		if(NewRect.x < OldClipper.x)
		{
			NewRect.w -= OldClipper.x-NewRect.x;
			NewRect.x = OldClipper.x;
		}
		if(NewRect.y < OldClipper.y)
		{
			NewRect.h -= OldClipper.y-NewRect.y;
			NewRect.y = OldClipper.y;
		}
		if(NewRect.x+NewRect.w > OldClipper.x+OldClipper.w)
			NewRect.w = OldClipper.x+OldClipper.w-NewRect.x;
		if(NewRect.y+NewRect.h > OldClipper.y+OldClipper.h)
			NewRect.h = OldClipper.y+OldClipper.h-NewRect.y;

		SDL_SetClipRect(Msg->Data.Draw.Target, &NewRect);
	}

	/* adjust according to offset of all children */
	CGUIMessage SubMessage = *Msg;
	switch(Msg->Type)
	{
			case CGUIMessage::DRAW:
				SubMessage.PositionX += OffsetX;
				SubMessage.PositionY += OffsetY;
			break;

			default:
				SubMessage.PositionX -= OffsetX;
				SubMessage.PositionY -= OffsetY;
			break;
	}

	/* pass on to all those for whom the mouse is over */
	GUIObjectPtr *P = Head;

	while(P)
	{
		SDL_Rect R = P->Object->GetArea();
//		R.x -= OffsetX<<1; R.y -= OffsetY<<1;

		switch(Msg->Type)
		{
			case CGUIMessage::DRAW:
				P->Object->Message(&SubMessage);
			break;

			case CGUIMessage::MOUSEOVER:
				SubMessage.Data.Mouse.Inside = 
					SubMessage.PositionX >= R.x && SubMessage.PositionX < R.x+R.w &&
					SubMessage.PositionY >= R.y && SubMessage.PositionY < R.y+R.h;

				P->Object->Message(&SubMessage);
			break;

			default:
				if(	SubMessage.PositionX >= R.x && SubMessage.PositionX < R.x+R.w &&
					SubMessage.PositionY >= R.y && SubMessage.PositionY < R.y+R.h)
						P->Object->Message(&SubMessage);
			break;
		}

		P = P->Next;
	}

	/* restore old rectangle if relevant*/
	if(Msg->Type == CGUIMessage::DRAW)
		SDL_SetClipRect(Msg->Data.Draw.Target, &OldClipper);
}
void CGUIObjectCollection::Move(int OffsetX, int OffsetY)
{
	GUIObjectPtr *P = Head;

	Target.x += OffsetX;
	Target.y += OffsetY;
	SubArea.x += OffsetX;
	SubArea.y += OffsetY;

	while(P)
	{
		P->Object->Move(OffsetX, OffsetY);
		P = P->Next;
	}
}

void CGUIObjectCollection::ChangeColour(SDL_Color &NewColour)
{
	GUIObjectPtr *P = Head;
	while(P)
	{
		P->Object->ChangeColour(NewColour);
		P = P->Next;
	}
}

CGUIObjectCollection::~CGUIObjectCollection() { ClearList(); }

void CGUIObjectCollection::InitialiseList(GUIObjectPtr *&H)
{
	H = NULL;
	SubArea = Target;
}
void CGUIObjectCollection::AddObject(GUIObjectPtr *&HPtr, CGUIObjectBase *obj)
{
	if(!obj) return;

	/* add to list */
	GUIObjectPtr *H = HPtr;
	HPtr = new GUIObjectPtr;
	HPtr->Next = H;
	HPtr->Object = obj;
}
void CGUIObjectCollection::ClearList(GUIObjectPtr *&HPtr, bool DeleteObjects)
{
	GUIObjectPtr *N;
	while(HPtr)
	{
		N = HPtr->Next;
		if(DeleteObjects) delete HPtr->Object;
		delete HPtr;
		HPtr = N;
	}
}

void CGUIObjectCollection::InitialiseList()
{
	InitialiseList(Head);
}
void CGUIObjectCollection::AddObject(CGUIObjectBase *obj)
{
	AddObject(Head, obj);
}
void CGUIObjectCollection::ClearList()
{
	ClearList(Head, true);
}

/*

	TEXT DISPLAY

*/
CGUIObjectText::~CGUIObjectText()
{
	free(TString);
}

CGUIObjectText::CGUIObjectText(CGUI &GParent, SDL_Surface *Static, const char *text, int size, SDL_Color &Fore, SDL_Rect &Targ, TextType Alignment)
{
	font = GParent.GetFont(size);
	TString = strdup(text);

	/* attach text image */
	SDL_Surface *TS = TTF_RenderText_Blended(font, text, Fore);
	if(!Static)
	{
		DisplaySurface = OptimiseSurface(TS, true);
		SDL_FreeSurface(TS);
	}

	/* calculate target rectangle */
	Target = Targ;

	int w, h;
	TTF_SizeText(font, text, &w, &h);
	Target.h = h;

	switch(Alignment)
	{
		default: break;
		case TT_CENTRE: Target.x += (Target.w - w) >> 1;	break;
		case TT_RIGHT: Target.x += Target.w - w;			break;
	}

	Target.w = w;

	/* if we're being asked to do a static draw, do so */
	if(Static)
	{
		SDL_BlitSurface(TS, NULL, Static, &Target);
		SDL_FreeSurface(TS);
		DisplaySurface = NULL;
	}
}

void CGUIObjectText::ChangeColour(SDL_Color &NewColour)
{
	/* makes no sense if static */
	if(!DisplaySurface) return;

	/* change text image */
	SDL_FreeSurface(DisplaySurface);
	SDL_Surface *TS = TTF_RenderText_Blended(font, TString, NewColour);
	DisplaySurface = OptimiseSurface(TS, true);
	SDL_FreeSurface(TS);
}

/*

	LINE

*/
CGUIObjectLine::CGUIObjectLine(SDL_Surface *Static, SDL_Color &Fore, int x1, int y1, int x2, int y2)
{
	Target.x = x1;
	Target.y = y1;
	Target.w = x2-x1; if(x2 < x1) { Target.x = x2; Target.w = -Target.w;}
	Target.h = y2-y1; if(y2 < y1) { Target.y = y2; Target.h = -Target.h;}

//	Target.w+=18;
//	Target.h+=18;
//	Target.x-=9;
//	Target.y-=9;

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

	DisplaySurface = SDL_CreateRGBSurface(Static ? 0 : SDL_SRCALPHA, Target.w+1, Target.h+1, 32, rmask, gmask, bmask, amask);
	if(Static)
		SDL_SetAlpha(DisplaySurface, 0, SDL_ALPHA_OPAQUE);

	/* quarter cosine table */
	const Uint8 ITable[512] =
	{
		0x00, 0x00, 0x01, 0x02, 0x03, 0x03, 0x04, 0x05, 0x06, 0x07, 0x07, 0x08, 0x09, 0x0a, 0x0a, 0x0b, 
		0x0c, 0x0d, 0x0e, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x12, 0x13, 0x14, 0x15, 0x15, 0x16, 0x17, 0x18, 
		0x19, 0x19, 0x1a, 0x1b, 0x1c, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x20, 0x21, 0x22, 0x23, 0x23, 0x24, 
		0x25, 0x26, 0x27, 0x27, 0x28, 0x29, 0x2a, 0x2a, 0x2b, 0x2c, 0x2d, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 
		0x31, 0x32, 0x33, 0x34, 0x34, 0x35, 0x36, 0x37, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3b, 0x3c, 0x3d, 
		0x3e, 0x3e, 0x3f, 0x40, 0x41, 0x41, 0x42, 0x43, 0x44, 0x44, 0x45, 0x46, 0x47, 0x47, 0x48, 0x49, 
		0x4a, 0x4a, 0x4b, 0x4c, 0x4d, 0x4d, 0x4e, 0x4f, 0x50, 0x50, 0x51, 0x52, 0x53, 0x53, 0x54, 0x55, 
		0x56, 0x56, 0x57, 0x58, 0x59, 0x59, 0x5a, 0x5b, 0x5b, 0x5c, 0x5d, 0x5e, 0x5e, 0x5f, 0x60, 0x61, 
		0x61, 0x62, 0x63, 0x63, 0x64, 0x65, 0x66, 0x66, 0x67, 0x68, 0x68, 0x69, 0x6a, 0x6b, 0x6b, 0x6c, 
		0x6d, 0x6d, 0x6e, 0x6f, 0x70, 0x70, 0x71, 0x72, 0x72, 0x73, 0x74, 0x74, 0x75, 0x76, 0x77, 0x77, 
		0x78, 0x79, 0x79, 0x7a, 0x7b, 0x7b, 0x7c, 0x7d, 0x7d, 0x7e, 0x7f, 0x7f, 0x80, 0x81, 0x81, 0x82, 
		0x83, 0x83, 0x84, 0x85, 0x86, 0x86, 0x87, 0x87, 0x88, 0x89, 0x89, 0x8a, 0x8b, 0x8b, 0x8c, 0x8d, 
		0x8d, 0x8e, 0x8f, 0x8f, 0x90, 0x91, 0x91, 0x92, 0x93, 0x93, 0x94, 0x94, 0x95, 0x96, 0x96, 0x97, 
		0x98, 0x98, 0x99, 0x9a, 0x9a, 0x9b, 0x9b, 0x9c, 0x9d, 0x9d, 0x9e, 0x9e, 0x9f, 0xa0, 0xa0, 0xa1, 
		0xa2, 0xa2, 0xa3, 0xa3, 0xa4, 0xa5, 0xa5, 0xa6, 0xa6, 0xa7, 0xa8, 0xa8, 0xa9, 0xa9, 0xaa, 0xaa, 
		0xab, 0xac, 0xac, 0xad, 0xad, 0xae, 0xae, 0xaf, 0xb0, 0xb0, 0xb1, 0xb1, 0xb2, 0xb2, 0xb3, 0xb4, 
		0xb4, 0xb5, 0xb5, 0xb6, 0xb6, 0xb7, 0xb7, 0xb8, 0xb8, 0xb9, 0xba, 0xba, 0xbb, 0xbb, 0xbc, 0xbc, 
		0xbd, 0xbd, 0xbe, 0xbe, 0xbf, 0xbf, 0xc0, 0xc0, 0xc1, 0xc1, 0xc2, 0xc2, 0xc3, 0xc3, 0xc4, 0xc4, 
		0xc5, 0xc5, 0xc6, 0xc6, 0xc7, 0xc7, 0xc8, 0xc8, 0xc9, 0xc9, 0xca, 0xca, 0xcb, 0xcb, 0xcc, 0xcc, 
		0xcd, 0xcd, 0xce, 0xce, 0xce, 0xcf, 0xcf, 0xd0, 0xd0, 0xd1, 0xd1, 0xd2, 0xd2, 0xd2, 0xd3, 0xd3, 
		0xd4, 0xd4, 0xd5, 0xd5, 0xd6, 0xd6, 0xd6, 0xd7, 0xd7, 0xd8, 0xd8, 0xd8, 0xd9, 0xd9, 0xda, 0xda, 
		0xda, 0xdb, 0xdb, 0xdc, 0xdc, 0xdc, 0xdd, 0xdd, 0xde, 0xde, 0xde, 0xdf, 0xdf, 0xe0, 0xe0, 0xe0, 
		0xe1, 0xe1, 0xe1, 0xe2, 0xe2, 0xe2, 0xe3, 0xe3, 0xe4, 0xe4, 0xe4, 0xe5, 0xe5, 0xe5, 0xe6, 0xe6, 
		0xe6, 0xe7, 0xe7, 0xe7, 0xe8, 0xe8, 0xe8, 0xe9, 0xe9, 0xe9, 0xe9, 0xea, 0xea, 0xea, 0xeb, 0xeb, 
		0xeb, 0xec, 0xec, 0xec, 0xec, 0xed, 0xed, 0xed, 0xee, 0xee, 0xee, 0xee, 0xef, 0xef, 0xef, 0xf0, 
		0xf0, 0xf0, 0xf0, 0xf1, 0xf1, 0xf1, 0xf1, 0xf2, 0xf2, 0xf2, 0xf2, 0xf3, 0xf3, 0xf3, 0xf3, 0xf3, 
		0xf4, 0xf4, 0xf4, 0xf4, 0xf5, 0xf5, 0xf5, 0xf5, 0xf5, 0xf6, 0xf6, 0xf6, 0xf6, 0xf6, 0xf7, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xfa, 
		0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfc, 0xfc, 
		0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 
		0xfd, 0xfd, 0xfd, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 
		0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe
	};

	if(Target.w || Target.h)
	{
	SDL_LockSurface(DisplaySurface);

	if(Target.w > Target.h)
	{
		int XPos = x1 - Target.x, Inc = (x2 > x1) ? 1 : -1;
		int YPos = (y1 - Target.y) << 16;
		int Adder = ((y2 - y1) << 16) / (x2 - x1);

		/* we know XPos always to be valid by virtue of the dimensions passed to CreateRGBSurface */
		XPos -= Inc;
		while(XPos != (x2 - Target.x))
		{
			XPos += Inc;
			if( (YPos >> 16) >= 0 && (YPos >> 16) <= Target.h)
			{
				((Uint8 *)DisplaySurface->pixels)[(YPos >> 16)*DisplaySurface->pitch + (XPos << 2) + 0] = Fore.r;
				((Uint8 *)DisplaySurface->pixels)[(YPos >> 16)*DisplaySurface->pitch + (XPos << 2) + 1] = Fore.g;
				((Uint8 *)DisplaySurface->pixels)[(YPos >> 16)*DisplaySurface->pitch + (XPos << 2) + 2] = Fore.b;
				((Uint8 *)DisplaySurface->pixels)[(YPos >> 16)*DisplaySurface->pitch + (XPos << 2) + 3] = ITable[((YPos >> 7)&0x1ff)^0x1ff];
			}

			if( (YPos >> 16) >= -1 && (YPos >> 16) < Target.h)
			{
				((Uint8 *)DisplaySurface->pixels)[((YPos >> 16)+1)*DisplaySurface->pitch + (XPos << 2) + 0] = Fore.r;
				((Uint8 *)DisplaySurface->pixels)[((YPos >> 16)+1)*DisplaySurface->pitch + (XPos << 2) + 1] = Fore.g;
				((Uint8 *)DisplaySurface->pixels)[((YPos >> 16)+1)*DisplaySurface->pitch + (XPos << 2) + 2] = Fore.b;
				((Uint8 *)DisplaySurface->pixels)[((YPos >> 16)+1)*DisplaySurface->pitch + (XPos << 2) + 3] = ITable[(YPos >> 7)&0x1ff];
			}

			YPos += Adder;
		}
	}
	else
	{
		int YPos = y1 - Target.y, Inc = (y2 > y1) ? 1 : -1;
		int XPos = (x1 -Target.x) << 16;
		int Adder = ((x2 - x1) << 16) / (y2 - y1);

		YPos -= Inc;
		while(YPos != (y2 - Target.y))
		{
			YPos += Inc;
			if( (XPos >> 16) >= 0 && (XPos >> 16) <= Target.w)
			{
				((Uint8 *)DisplaySurface->pixels)[YPos*DisplaySurface->pitch + ((XPos >> 16) << 2) + 0] = Fore.r;
				((Uint8 *)DisplaySurface->pixels)[YPos*DisplaySurface->pitch + ((XPos >> 16) << 2) + 1] = Fore.g; 
				((Uint8 *)DisplaySurface->pixels)[YPos*DisplaySurface->pitch + ((XPos >> 16) << 2) + 2] = Fore.b;
				((Uint8 *)DisplaySurface->pixels)[YPos*DisplaySurface->pitch + ((XPos >> 16) << 2) + 3] = ITable[((XPos >> 7)&0x1ff)^0x1ff];
			}

			if( (XPos >> 16) >= -1 && (XPos >> 16) < Target.w)
			{
				((Uint8 *)DisplaySurface->pixels)[YPos*DisplaySurface->pitch + (((XPos >> 16) + 1) << 2) + 0] = Fore.r;
				((Uint8 *)DisplaySurface->pixels)[YPos*DisplaySurface->pitch + (((XPos >> 16) + 1) << 2) + 1] = Fore.g;
				((Uint8 *)DisplaySurface->pixels)[YPos*DisplaySurface->pitch + (((XPos >> 16) + 1) << 2) + 2] = Fore.b;
				((Uint8 *)DisplaySurface->pixels)[YPos*DisplaySurface->pitch + (((XPos >> 16) + 1) << 2) + 3] = ITable[(XPos >> 7)&0x1ff];
			}

			XPos += Adder;
		}
	}

	SDL_UnlockSurface(DisplaySurface);
	}

	/* do a static draw if requested */
	if(Static)
	{
		SDL_BlitSurface(DisplaySurface, NULL, Static, &Target);
		SDL_FreeSurface(DisplaySurface);
		DisplaySurface = NULL;
	}
	else
	{
		/* convert surface to best format */
		SDL_Surface *TS = DisplaySurface;
		DisplaySurface = OptimiseSurface(TS);
		SDL_FreeSurface(TS);
	}
}

/*

	UNFILLED OUTLINE

*/
CGUIObjectOutline::CGUIObjectOutline(SDL_Surface *Static, SDL_Color &Fore, SDL_Rect &Area)
{
	Target = Area;
	DisplaySurface = NULL;
	OffsetX = OffsetY = 0;

	InitialiseList();

	CGUIObjectLine *Outline;
	Outline = new CGUIObjectLine(Static, Fore, Target.x, Target.y, Target.x+Target.w, Target.y);
	AddObject(Outline);
	Outline = new CGUIObjectLine(Static, Fore, Target.x+Target.w, Target.y, Target.x+Target.w, Target.y+Target.h);
	AddObject(Outline);
	Outline = new CGUIObjectLine(Static, Fore, Target.x, Target.y+Target.h, Target.x+Target.w, Target.y+Target.h);
	AddObject(Outline);
	Outline = new CGUIObjectLine(Static, Fore, Target.x, Target.y, Target.x, Target.y+Target.h);
	AddObject(Outline);

	SubArea.w += 2;
	SubArea.h += 2;
	SubArea.x --;
	SubArea.y --;
}

/*

	FILLED RECTANGLE

*/
CGUIObjectRectangle::CGUIObjectRectangle(SDL_Surface *Static, SDL_Color &Fore, SDL_Rect &Area)
{
	Target = Area;
	Colour = Fore;
	DisplaySurface = NULL;
}

void CGUIObjectRectangle::Message(CGUIMessage *m)
{
	CGUIObjectBase::Message(m);
	if(m->Type == CGUIMessage::DRAW)
	{
		SDL_Rect TRect = Target;
		TRect.x += m->PositionX;
		TRect.y += m->PositionY;
		SDL_FillRect(m->Data.Draw.Target, &TRect, SDL_MapRGB(m->Data.Draw.Target->format, Colour.r, Colour.g, Colour.b));
	}
}

/*

	BUTTON of any description

*/
CGUIObjectButton::~CGUIObjectButton()
{
	if(Up[0])
	{
		int c = 4;
		while(c--)
		{
			delete Up[c];
			delete Down[c];
		}
	}

	delete Wording;
}

CGUIObjectButton::CGUIObjectButton(CGUIObjectBase *MessageTarget, Uint32 TargetID, CGUI &GParent, SDL_Surface *Static, const char *text, int size, SDL_Color &Fore, SDL_Rect &Targ, bool bordered)
{
	DisplaySurface = NULL;
	Over = Pressed = false;

	/* prepare click message now */
	ClickMessage.Type = CGUIMessage::EVENT;
	ClickMessage.Data.Event.Target = MessageTarget;
	ClickMessage.Data.Event.Message = TargetID;
	Parent = &GParent;

	/* create central wording (which is actual text in this constructor */
	Wording = new CGUIObjectText(GParent, bordered ? NULL : Static, text, size, Fore, Targ, TT_LEFT);
	Target = Wording->GetArea();

	Target.x -= 2;
	Target.w += 4;
	Target.y -= 2;
	Target.h += 4;

	SDL_Color White = {0xff, 0xff, 0xff}, Gray = {0x80, 0x80, 0x80}, DeepGray = {0x40, 0x40, 0x40};
	MouseOver = new CGUIObjectRectangle(NULL, DeepGray, Target);

	if(bordered)
	{
		Up[0] = new CGUIObjectLine(NULL, White, Target.x, Target.y, Target.x+Target.w, Target.y);
		Up[1] = new CGUIObjectLine(NULL, Gray, Target.x+Target.w, Target.y, Target.x+Target.w, Target.y+Target.h);
		Up[2] = new CGUIObjectLine(NULL, Gray, Target.x, Target.y+Target.h, Target.x+Target.w, Target.y+Target.h);
		Up[3] = new CGUIObjectLine(NULL, White, Target.x, Target.y, Target.x, Target.y+Target.h);

		Down[0] = new CGUIObjectLine(NULL, Gray, Target.x, Target.y, Target.x+Target.w, Target.y);
		Down[1] = new CGUIObjectLine(NULL, White, Target.x+Target.w, Target.y, Target.x+Target.w, Target.y+Target.h);
		Down[2] = new CGUIObjectLine(NULL, White, Target.x, Target.y+Target.h, Target.x+Target.w, Target.y+Target.h);
		Down[3] = new CGUIObjectLine(NULL, Gray, Target.x, Target.y, Target.x, Target.y+Target.h);
	}
	else
		Up[0] = NULL;

	Pressed = false;
}

void CGUIObjectButton::Message(CGUIMessage *msg)
{
	switch(msg->Type)
	{
		case CGUIMessage::DRAW:
			if(Pressed && Over)
			{
				CGUIMessage PMessage = *msg;
				PMessage.PositionX++;
				PMessage.PositionY++;

				if(Up[0])
				{
					if(Over) MouseOver->Message(&PMessage);

					int c = 4;
					while(c--)
						Down[c]->Message(msg);
				}
				else
					if(Over) MouseOver->Message(msg);

				Wording->Message(&PMessage);
			}
			else
			{
				if(Over) MouseOver->Message(msg);
				Wording->Message(msg);
				if(Up[0])
				{
					int c = 4;
					while(c--)
						Up[c]->Message(msg);
				}
			}

		break;

		case CGUIMessage::BUTTONS:{
			bool OPressed = Pressed;
			Pressed = (msg->Buttons&SDL_BUTTON_LMASK) ? true : false;

			if(msg->Data.Button.Changes&SDL_BUTTON_LMASK)
			{
				if(!Pressed && OPressed && Up[0])
					Parent->PostMessage(&ClickMessage);

				if(Pressed && !Up[0])
					Parent->PostMessage(&ClickMessage);
			}
		}break;

		case CGUIMessage::MOUSEOVER:
			if(msg->Data.Mouse.Inside && !Over && Pressed)
				Pressed = msg->Buttons&SDL_BUTTON_LMASK;

			Over = msg->Data.Mouse.Inside;
		break;
	}
}

void CGUIObjectButton::ChangeColour(SDL_Color &NewColour)
{
	Wording->ChangeColour(NewColour);
}

void CGUIObjectButton::Move(int OffsetX, int OffsetY)
{
	MouseOver->Move(OffsetX, OffsetY);
	Wording->Move(OffsetX, OffsetY);

	if(Up[0])
	{
		int c = 4;
		while(c--)
		{
			Up[c]->Move(OffsetX, OffsetY);
			Down[c]->Move(OffsetX, OffsetY);
		}
	}

	Target.x += OffsetX;
	Target.y += OffsetY;
}

/*

	SCROLL BAR / ANALOGUE ADJUSTER

*/
CGUIObjectAnalogue::CGUIObjectAnalogue(CGUI &GParent, SDL_Surface *Static, SDL_Rect &Targ, bool horiz, CGUIObjectBase *Chd)
{
	DisplaySurface = NULL;
	Target = Targ;
	OffsetX = OffsetY = 0;
	CursorTied = false;
	Value = 0;
	Horizontal = horiz;
	InitialiseList();

	SDL_Color DGray = {0x80, 0x80, 0x80};
	CGUIObjectOutline *Outline = new CGUIObjectOutline(Static, DGray, Target);
	AddObject(Outline);

	SliderTarget = Target;

	if(Horizontal)
	{
		SliderTarget.y += SliderTarget.h-16;
		SliderTarget.h = 16;
	}
	else
	{
		SliderTarget.x += SliderTarget.w-16;
		SliderTarget.w = 16;
	}

	SDL_Color Gray = {0xc0, 0xc0, 0xc0};
	Outline = new CGUIObjectOutline(Static, Gray, SliderTarget);
	AddObject(Outline);

	SDL_Color White = {0xff, 0xff, 0xff};
	SDL_Rect T = SliderTarget;
	SubArea = Targ;

	if(Horizontal)
	{
		T.w = 32;
		SubArea.h -= 18;
		SubArea.y++;
		SubArea.x++;
		SubArea.w -= 2;
	}
	else
	{
		T.h = 32;
		SubArea.w -= 18;
		SubArea.x++;
		SubArea.y++;
		SubArea.h -= 2;
	}

	Slider = new CGUIObjectRectangle(NULL, White, T);

	if(Child = Chd)
	{
		AddObject(Child);

		SDL_Rect CRect = Child->GetArea();
		if(Horizontal)
			ScrollArea = CRect.w - Target.w;
		else
			ScrollArea = CRect.h - Target.h;

		if(ScrollArea < 0)
			ScrollArea = 0;
	}
	else
		ScrollArea = 0;
}

CGUIObjectAnalogue::~CGUIObjectAnalogue()
{
	delete Slider;
}

void CGUIObjectAnalogue::Message(CGUIMessage *m)
{
	CGUIObjectCollection::Message(m);

	switch(m->Type)
	{
		case CGUIMessage::BUTTONS:
			if(CursorTied ||
				(
					m->PositionX >= SliderTarget.x && m->PositionX < SliderTarget.x+SliderTarget.w &&
					m->PositionY >= SliderTarget.y && m->PositionY < SliderTarget.y+SliderTarget.h
				)
			)
			{
				if(CursorTied = (m->Buttons&SDL_BUTTON_LMASK))
					Value = Horizontal ?
						(float)(m->PositionX - Target.x - 16) / (float)(Target.w-32) :
						(float)(m->PositionY - Target.y - 16) / (float)(Target.h-32);

				if(m->Data.Button.Changes&SDL_BUTTON(SDL_BUTTON_WHEELUP)) Value += 0.1f; 
				if(m->Data.Button.Changes&16) Value -= 0.1f;
			}
		break;

		case CGUIMessage::MOUSEOVER:
			if(CursorTied)
			{
				if(!m->Buttons&SDL_BUTTON_LMASK)
					CursorTied = false;
				else				
					Value = Horizontal ?
						(float)(m->PositionX - Target.x - 16) / (float)(Target.w-32) :
						(float)(m->PositionY - Target.y - 16) / (float)(Target.h-32);
			}
		break;

		case CGUIMessage::DRAW:
		{
			/* slider */
			CGUIMessage SLMessage = *m;
			if(Horizontal)
				SLMessage.PositionX += (int)(Value*(Target.w-32));
			else
				SLMessage.PositionY += (int)(Value*(Target.h-32));

			Slider->Message(&SLMessage);
		}
		break;
	}

	if(Value < 0) Value = 0;
	if(Value > 1) Value = 1;

	if(Horizontal)
		OffsetX = -(int)(Value*ScrollArea);
	else
		OffsetY = -(int)(Value*ScrollArea);
}

SDL_Rect CGUIObjectAnalogue::GetVisibleArea()
{
	SDL_Rect VisArea = SubArea;
	VisArea.x -= OffsetX;
	VisArea.y -= OffsetY;
	return VisArea;
}

/*

	SDL_ttf overlay functions that understand tabs
 
*/
int mTTF_SizeText(TTF_Font *font, const char *text, int *w, int *h)
{
	const char *eptr;
	char *wptr;
	char TBuffer[512];
	int TrueW = 0, TrueH = 0;

	eptr = text;
	wptr = TBuffer;
	while(*eptr)
	{
		while(*eptr && *eptr != '\t')
			*wptr++ = *eptr++;
		*wptr = '\0';

		int NewW, NewH;
		TTF_SizeText(font, TBuffer, &NewW, &NewH);

		TrueW += NewW;
		if(NewH > TrueH) TrueH = NewH;

		if(*eptr)
		{
			eptr++;
			wptr = TBuffer;
		}
	}

	*w = TrueW;
	*h = TrueH;

	return 0;
}

SDL_Surface *_mTTFi_RenderText(bool shaded, const char *text, SDL_Color fg, SDL_Color bg)
{
	int XPos = 0;
	const char *tptr = text;
	while(*tptr)
	{
	}

	return NULL;
}

SDL_Surface *mTTF_RenderText_Blended(TTF_Font *font, const char *text, SDL_Color fg)
{
	return _mTTFi_RenderText(false, text, fg, fg);
}

SDL_Surface *mTTF_RenderText_Shaded(TTF_Font *font, const char *text, SDL_Color fg, SDL_Color bg)
{
	return _mTTFi_RenderText(true, text, fg, bg);
}
/*

	TEXT BOX

*/
CGUIObjectTextBox::CGUIObjectTextBox(CGUI &GParent, SDL_Surface *Static, const char *text, int size, SDL_Color &TextColour, SDL_Rect &Targ, TextType AL)
{
	DisplaySurface = NULL;
	Target = Targ;
	Text = strdup(text);
	font = GParent.GetFont(size);
	FontColour = TextColour;
	Alignment = AL;
	Alpha = true;

//	if(Static)
//	{
//		Render(Static);
//	}
//	else
//	{
		/* determine true dimensions */
		Render(NULL);

		/* create bitmap */
		DisplaySurface = OptimalSurface(Target.w, Target.h, true);
		SDL_SetAlpha(DisplaySurface, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
		Render(DisplaySurface, -Target.x, -Target.y);
//	}
}

CGUIObjectTextBox::CGUIObjectTextBox(CGUI &GParent, SDL_Surface *Static, const char *text, int size, SDL_Color &TextColour, SDL_Color &BackGround, SDL_Rect &Targ, TextType AL)
{
	DisplaySurface = NULL;
	Target = Targ;
	Text = strdup(text);
	font = GParent.GetFont(size);
	FontColour = TextColour;
	BackColour = BackGround;
	Alignment = AL;
	Alpha = false;

//	if(Static)
//	{
//		Render(Static);
//	}
//	else
//	{
		/* determine true dimensions */
		Render(NULL);

		/* create bitmap */
		DisplaySurface = OptimalSurface(Target.w, Target.h);

		Render(DisplaySurface, -Target.x, -Target.y);
//	}
}

CGUIObjectTextBox::~CGUIObjectTextBox()
{
	free(Text);
}

char *CGUIObjectTextBox::RenderLine(SDL_Surface *s, char *Start, int OffsetX, int OffsetY)
{
	/* determine how much can be fitted on this line */
	char IntermediateBuffer[512], *WTarget = IntermediateBuffer;

	memset(IntermediateBuffer, '\0', 512);

	char *End = Start, *LastEnd = Start;
	while(*End && *End != '\n')
	{
		/* copy beyond current space and up to next space, underscore or tab */
		while((*End == ' ' || *End == '_') && *End)
		{
			*WTarget++ = *End;
			End++;
		}
		while(*End != ' ' && *End != '\n' && *End != '_' && *End)
		{
			*WTarget++ = *End;
			End++;
		}

		int w, h;
		mTTF_SizeText(font, IntermediateBuffer, &w, &h);

		/* have we gone too far? */
		bool TooFar = false;
		if(w > Target.w) TooFar = true;

		if(TooFar)
		{
			/* receed to last space, if that isn't the very start */
			if(LastEnd != Start)
			{
				WTarget -= End - LastEnd;
				End = LastEnd;
				*WTarget = '\0';
				break;
			}
			else
			{
				/* going to have to split this word */
				while(w > Target.w)
				{
					*WTarget-- = '\0';
					End--;
					mTTF_SizeText(font, IntermediateBuffer, &w, &h);
				}

				break;
			}
		}

		LastEnd = End;
	}

	/* OutputBuffer now contains one line of text */
	if(s)
	{
		SDL_Surface *TS = Alpha ? TTF_RenderText_Blended(font, IntermediateBuffer, FontColour) : TTF_RenderText_Shaded(font, IntermediateBuffer, FontColour, BackColour);

		if(TS)
		{
			int XOffs;
			switch(Alignment)
			{
				case TT_LEFT: XOffs = 0; break;
				case TT_RIGHT: XOffs = Target.w - TS->w; break;
				case TT_CENTRE: XOffs = (Target.w - TS->w) >> 1; break;
			}

			SDL_SetAlpha(TS, 0, SDL_ALPHA_OPAQUE);
			SDL_SetColorKey(TS, SDL_SRCCOLORKEY, 0);
			SDL_Rect T;
			T.x = Target.x + OffsetX + XOffs;
			T.y = Target.y + OffsetY;
			T.w = TS->w; T.h = TS->h;
			SDL_BlitSurface(TS, NULL, s, &T);

			if(TS->w > MaxW) MaxW = TS->w;

			SDL_FreeSurface(TS);
		}
	}
	else
	{
		int w, h;
		mTTF_SizeText(font, IntermediateBuffer, &w, &h);
		if(w > MaxW) MaxW = w;
		if(h+OffsetY > Target.h) Target.h = h+OffsetY;
	}

	Start += strlen(IntermediateBuffer);
	while(*Start == ' ') Start++;
	return Start;
}

void CGUIObjectTextBox::Render(SDL_Surface *s, int OffsetX, int OffsetY)
{
	char *TPtr = Text;
	int PosY = 0, EndH = TTF_FontLineSkip(font);
	MaxW = 0;

	if(!s)
		Target.h = 0;

	while(*TPtr)
	{
		/* handle any control characters first */
		bool Special = true;

		while(Special)
		{
			switch(*TPtr)
			{
				default: Special = false; break;
				case '\n':
					PosY += TTF_FontLineSkip(font);
					TPtr++;
				break;
			}
		}

		/* now output the current line */
		TPtr = RenderLine(s, TPtr, OffsetX, OffsetY+PosY);
		PosY += TTF_FontLineSkip(font);

		/* this is one newline 'for free', so... */
		if(*TPtr == '\n') TPtr++;
	}
}

/*

	INLINE GRAPHIC

*/
CGUIObjectGraphic::CGUIObjectGraphic(CGUI &GParent, SDL_Surface *Static, SDL_Rect &Targ, int GID)
{
	Target = Targ;
	DisplaySurface = NULL;
	GraphicID = GID;
	Parent = &GParent;

	SDL_Rect Dims = GParent.GetGraphicDimensions(GraphicID);
	Target.w = Dims.w;
	Target.h = Dims.h;
}

void CGUIObjectGraphic::Message(CGUIMessage *m)
{
	switch(m->Type)
	{
		default: CGUIObjectBase::Message(m); break;

		case CGUIMessage::DRAW:
			Parent->AddGraphic(m->Data.Draw.Target, GraphicID, m->PositionX+Target.x, m->PositionY+Target.y);
		break;
	}
}

#endif
