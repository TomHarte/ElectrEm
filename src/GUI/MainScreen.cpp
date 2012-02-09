#if !defined(USE_NATIVE_GUI) && !defined(NO_GUI)

#include "MainScreen.h"
#include "AboutScreen.h"
#include "FileScreen.h"

#define BACK_MSG	0
#define QUIT_MSG	1

#define TAB1		2
#define TAB2		3
#define TAB3		4
#define TAB4		5
#define TAB5		6
#define TAB6		7
#define TAB7		8
#define TAB8		9

int TabID = 7;

CGUIMainDlg::~CGUIMainDlg()
{
	if(TabLine) SDL_FreeSurface(TabLine);
	if(Child) delete Child;
	ClearList(Tabs);
}

#define AddTab(s)\
	NewButton = new CGUIObjectButton(this, TAB1+NumTabs, Par, NULL, s, 14, White, Words, false);\
	AddObject(NewButton); AddObject(Tabs, NewButton);\
	NumTabs++;\
	Words.x += NewButton->GetArea().w + 10;


/* i.e. tabs at top of display, 'back to emulator' button */
CGUIMainDlg::CGUIMainDlg(CGUI &Par, SDL_Surface *St, SDL_Rect &FullArea)
{
	DisplaySurface = NULL;
	OffsetX = OffsetY = 0;
	Parent = &Par;
	InitialiseList();
	SDL_Color White = {0xff, 0xff, 0xff};
	Child = NULL;
	Static = St;
	SubArea = Target = FullArea;

	/* Back To Electron button */
	CGUIObjectButton *BackButton = new CGUIObjectButton(this, BACK_MSG, Par, NULL, "Back to Electron", 14, White, FullArea, true);
	SDL_Rect T = BackButton->GetArea();
	T.x = (FullArea.w - T.w) - T.x - 5;
	T.y = 5;
	BackButton->Move(T.x, T.y);
	AddObject(BackButton);

	/* Quit button */
	BackButton = new CGUIObjectButton(this, QUIT_MSG, Par, NULL, "Quit", 14, White, FullArea, true);
	SDL_Rect T2 = BackButton->GetArea();
	BackButton->Move((T.x - T2.w) - T2.x - 5, 5);
	AddObject(BackButton);

	/* tab buttons */
	SDL_Rect Words = FullArea;
	Words.x += 8;
	Words.y += 8;

	CGUIObjectButton *NewButton;
	InitialiseList(Tabs);
	NumTabs = 0;

	AddTab("File");
	AddTab("Media");
	AddTab("Hardware");
	AddTab("ROMs");
	AddTab("Keyboard");
	AddTab("Joystick(s)");
	AddTab("Debug");
	AddTab("About");

	Words = NewButton->GetArea();

	TabLineTarget.x = FullArea.x;
	TabLineTarget.w = FullArea.w;
	TabLineTarget.y = Words.y;
	TabLineTarget.h = Words.h;

	/* screen outline */
	CGUIObjectLine *NewLine;
	
	FullArea.y = Words.y+Words.h;
	FullArea.h -= FullArea.y;
	NewLine = new CGUIObjectLine(Static, White, FullArea.x, FullArea.y, FullArea.x, FullArea.y+FullArea.h-1);
	AddObject(NewLine);
	NewLine = new CGUIObjectLine(Static, White, FullArea.x+FullArea.w-1, FullArea.y, FullArea.x+FullArea.w-1, FullArea.y+FullArea.h-1);
	AddObject(NewLine);
	NewLine = new CGUIObjectLine(Static, White, FullArea.x, FullArea.y+FullArea.h-1, FullArea.x+FullArea.w, FullArea.y+FullArea.h-1);
	AddObject(NewLine);

	/* tabs */
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

	SDL_Surface *C = SDL_CreateRGBSurface(SDL_SRCALPHA, FullArea.w, Words.h, 32, rmask, gmask, bmask, amask);
	TabLine = SDL_DisplayFormatAlpha(C);
	SDL_FreeSurface(C);

	SDL_FillRect(TabLine, NULL, SDL_MapRGBA(TabLine->format, 0xff, 0xff, 0xff, 0x40));
	SubArea = FullArea;
	SubArea.y += 5;
	SubArea.h -= 10;
	SubArea.x += 5;
	SubArea.w -= 10;

	BringToFore(TabID);
}

void CGUIMainDlg::BringToFore(int id)
{
	TabID = id;

	/* clear static area */
	SDL_FillRect(Static, &SubArea, SDL_MapRGB(Static->format, 0, 0, 0));

	/* child */
	if(Child) {delete Child; Child = NULL; }
	Child = GetChild(id);

	/* graphics */
	id = NumTabs - id-1;

	/* clear TabLine */
	SDL_FillRect(TabLine, NULL, SDL_MapRGBA(TabLine->format, 0, 0, 0, 0));

	SDL_Color White = {0xff, 0xff, 0xff}, Gray = {0x80, 0x80, 0x80};

	CGUIObjectLine *NewLine;
	GUIObjectPtr *TPtr = Tabs;
	SDL_Rect TArea;

	int c = 0;
	while(TPtr)
	{
		SDL_Rect Area = TPtr->Object->GetArea();
		if(id == c) TArea = Area;

		/* produce lines */
		NewLine = new CGUIObjectLine(TabLine, (id == c) ? White : Gray, Area.x, 0, Area.x-5, Area.h-1);
		delete NewLine;
		NewLine = new CGUIObjectLine(TabLine, (id == c) ? White : Gray, Area.x+Area.w, 0, Area.x+Area.w+4, Area.h-1);
		delete NewLine;
		NewLine = new CGUIObjectLine(TabLine, (id == c) ? White : Gray, Area.x, 0, Area.x+Area.w, 0);
		delete NewLine;

		/* recolour text */
		((CGUIObjectText *)TPtr->Object)->ChangeColour((id == c) ? White : Gray);

		/* advance */
		c++;
		TPtr = TPtr->Next;
	}

	/* top line */
	NewLine = new CGUIObjectLine(TabLine, White, TabLineTarget.x, TArea.h-1, TArea.x-5, TArea.h-1);
	delete NewLine;
	NewLine = new CGUIObjectLine(TabLine, White, TArea.x+TArea.w+5, TArea.h-1, TabLineTarget.x+TabLineTarget.w, TArea.h-1);
	delete NewLine;
}

void CGUIMainDlg::Message(CGUIMessage *m)
{
	CGUIObjectCollection::Message(m);
	if(Child) Child->Message(m);

	switch(m->Type)
	{
		case CGUIMessage::EVENT:
			if(m->Data.Event.Target == this)
			{
				switch(m->Data.Event.Message)
				{
					case BACK_MSG: Parent->SignalQuit(); break;
					case QUIT_MSG: Parent->SignalQuit(true); break;

					default:
						BringToFore(m->Data.Event.Message-TAB1);
					break;
				}
			}
		break;

		case CGUIMessage::DRAW:
			SDL_BlitSurface(TabLine, NULL, m->Data.Draw.Target, &TabLineTarget);
		break;
	}
}

CGUIObjectBase *CGUIMainDlg::GetChild(int id)
{
	switch(id)
	{
		default: return NULL;

		case 0: return new CGUIFileDlg(*Parent, Static, SubArea);
		case 7: return new CGUIAboutDlg(*Parent, Static, SubArea);
	}
}

#endif
