#if !defined(USE_NATIVE_GUI) && !defined(NO_GUI)

#include "AboutScreen.h"
#include "GFXDefines.h"

#define ABOUT_TEXT\
	"ElectrEm is an open source emulator of the Acorn Electron microcomputer, written in C++. It is intended to be portable, and to that end makes use of the SDL, SDL_ttf and zLib libraries.\n\nThis, the 'Future' version of the emulator is a radical reworking of the original intended to provide a cleaner, faster code base which is simultaneously more accurate in its emulation. The emulation is 'event based' and respects all timing issues of the original hardware.\n\nIt also supports an 'enhanced' operation mode, whereby extended data is piggybacked onto the machine emulation in order to produce an enhanced output display.\n\nSupported file formats at this time are UEF and, for Windows users only, CSW. Once functioning, disc drive emulation will support UEF, ADL/ADM/ADF/SSD/DSD, DSK and FDI. It is hoped that tape formats to be supported in the future will include T2P and WAV.\n"

#define COPYRIGHT_TEXT\
	"Emulator and GUI source code by Thomas Harte (ThomasHarte@lycos.co.uk). Modified and improved (including MacOS version) by Ewen Roberts. CSW.dll (distributed with Windows version only) by Fraser Ross. FDI reading code by Toni Wilen. All are provided 'as-is', without any express or implied warranty.\n\nThe Acorn ROMs are copyright PACE, and are distributed with their knowledge.\n\nSDL and SDL_ttf are distributed under the terms of the GNU LGPL license:\n\thttp://www.gnu.org/copyleft/lesser.html\n\nSource for those libraries is available from the libraries page at the SDL website:\n\thttp://www.libsdl.org/\n\nzLib is (C) 1995-2002 Jean-loup Gailly and Mark Adler and is provided 'as-is', without any express or implied warranty.\n\n'Transparent Electron' image and emulator name by Ian Marshall.\n\nThe following, listed in alphabetical order, have also aided the project in one way or another:\n\n\t- gARetH baBB\n\t- John Belson\n\t- David Boddie\n\t- Paul Boddie\n\t- Ricky Grant\n\t- R. Henderson\n\t- Wouter Hobers\n\t- Simon Irwin\n\t- Marcel de Kogel\n\t- Dave M\n\t- David Raven\n\t- Mark Shackman\n\t- Partrick Shoenmakers\n\t- Paul Walker\n\t- John Wike\n"

class CGUIAboutDlgContents: public CGUIObjectCollection
{
	public: CGUIAboutDlgContents(CGUI &, SDL_Surface *Static, SDL_Rect &FullArea);
};

CGUIAboutDlgContents::CGUIAboutDlgContents(CGUI &Par, SDL_Surface *Static, SDL_Rect &FullArea)
{
	OffsetX = OffsetY = 0;
	InitialiseList();
	DisplaySurface = NULL;

	SubArea = Target = FullArea;
	SubArea.h = 0;

	SDL_Color White = {0xff, 0xff, 0xff}, Black = {0, 0, 0};
	CGUIObjectTextBox *Box = new CGUIObjectTextBox(Par, Static, "Welcome to ElectrEm Future (3rd binary release)", 24, White, Black, Target, TT_LEFT);
	AddObject(Box);
	Target.y += Box->GetArea().h;
	SubArea.h += Box->GetArea().h;

	Box = new CGUIObjectTextBox(Par, Static, ABOUT_TEXT, 14, White, Black, Target, TT_LEFT);
	AddObject(Box);
	Target.y += Box->GetArea().h;
	SubArea.h += Box->GetArea().h;

	Box = new CGUIObjectTextBox(Par, Static, "Copyright", 18, White, Black, Target, TT_LEFT);
	AddObject(Box);
	Target.y += Box->GetArea().h;
	SubArea.h += Box->GetArea().h;

	Box = new CGUIObjectTextBox(Par, Static, COPYRIGHT_TEXT, 14, White, Black, Target, TT_LEFT);
	AddObject(Box);
	Target.y += Box->GetArea().h;
	SubArea.h += Box->GetArea().h;

	CGUIObjectGraphic *Graphic = new CGUIObjectGraphic(Par, Static, Target, GFX_ELECTRON);
	AddObject(Graphic);
	Graphic->Move((Target.w - Graphic->GetArea().w) >> 1, 0);
	Target.y += Graphic->GetArea().h;
	SubArea.h += Graphic->GetArea().h;

	Target = SubArea;
}

CGUIAboutDlg::CGUIAboutDlg(CGUI &Par, SDL_Surface *Static, SDL_Rect &FullArea)
{
	OffsetX = OffsetY = 0;
	InitialiseList();
	DisplaySurface = NULL;

	/* analogue slider into about and credits display */
	SubArea = Target = FullArea;

	SDL_Rect Inside = Target;
	Inside.w -= 18;
	Inside.x += 1;
	CGUIAboutDlgContents *Cont = new CGUIAboutDlgContents(Par, NULL, Inside);

	/* slider */
	CGUIObjectAnalogue *MessageArea = new CGUIObjectAnalogue(Par, Static, Target, false, Cont);
	AddObject(MessageArea);
}
#endif
