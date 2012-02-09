#if !defined(USE_NATIVE_GUI) && !defined(NO_GUI)

#include "FileScreen.h"
#include "GFXDefines.h"
#include "../HostMachine/HostMachine.h"
#include <string.h>
#include <malloc.h>

#define FILE_CLICK	0

class CGUIFileDlgFile: public CGUIObjectCollection
{
	public:
		CGUIFileDlgFile(CGUI &, void *Target, Uint32 Message, SDL_Surface *Static, SDL_Rect &FullArea, FileDesc *File);
		~CGUIFileDlgFile();
		void Message(CGUIMessage *Msg);

		bool TypeKnown();
		void DetermineType(char *path);

	private:
		Uint32 Graphic;
		bool UEFChecked;

		FileDesc *File;
		CGUI *Parent;
		CGUIObjectBase *Outline, *Text;
		bool Over;
		int IconX;
		CGUIMessage ClickMessage;
		void SetIcon(int Graph);
};

void CGUIFileDlgFile::Message(CGUIMessage *Msg)
{
	switch(Msg->Type)
	{
		case CGUIMessage::MOUSEOVER:{
			bool OOver = Over;
			Over = Msg->Data.Mouse.Inside && !(Msg->Buttons&SDL_BUTTON_LMASK);
			if(Over != OOver) SetIcon(Graphic);
		}break;

		case CGUIMessage::BUTTONS:
			if(Msg->Buttons&SDL_BUTTON_LMASK&Msg->Data.Button.Changes)
				Parent->PostMessage(&ClickMessage);
		break;
	}

	CGUIObjectCollection::Message(Msg);
}

CGUIFileDlgFile::CGUIFileDlgFile(CGUI &Par, void *Tget, Uint32 Message, SDL_Surface *Static, SDL_Rect &FullArea, FileDesc *F)
{
	File = F;
	Target = FullArea;
	DisplaySurface = NULL;
	InitialiseList();
	OffsetX = OffsetY = 0;
	Parent = &Par;

	ClickMessage.Type = CGUIMessage::EVENT;
	ClickMessage.Data.Event.Target = Tget;
	ClickMessage.Data.Event.Message = Message;
	ClickMessage.Data.Event.Source = F;

	if(File)
	{
		SDL_Rect TextArea = FullArea;
		SDL_Color White = {0xff, 0xff, 0xff};
		TextArea.y += 25;
		TextArea.h -= 25;
		TextArea.x += 3;
		TextArea.w -= 6;
		Text = new CGUIObjectTextBox(Par, NULL, File->Name, 14, White, TextArea, TT_CENTRE);

		int W = Text->GetArea().w;
		Text->Move((FullArea.w - W) >> 1, 0);
	}
	else
		Text = NULL;

	/* outline */
	SDL_Color OutlineCol = {0x6d, 0x85, 0xc9};
	Outline = new CGUIObjectRectangle(Static, OutlineCol, Target);
	Over = false;

	/* decide (start) graphic */
	if(!File)
		SetIcon(GFX_UPFOLDER);
	else
		if(File->Type == FileDesc::FD_DIRECTORY)
			SetIcon(GFX_FOLDER);
		else
		{
			char *KnownTypes[] = {"adf\a", "adl\a", "adm\a", "ssd\a", "dsd\a", "img\a", "csw\a", "fdi\a", NULL};
			if( GetHost()->ExtensionIncluded(File->Name, KnownTypes))
			{
				char *Tapes[] = {"csw\a", NULL};
				if( GetHost()->ExtensionIncluded(File->Name, Tapes))
					SetIcon(GFX_TAPE);
				else
					SetIcon(GFX_DISC);
			}
			else
				SetIcon(GFX_UNKNOWN);
		}

	UEFChecked = false;
	SetIcon(Graphic);
}

CGUIFileDlgFile::~CGUIFileDlgFile()
{
	delete[] Text;
}


void CGUIFileDlgFile::SetIcon(int Graph)
{
	Graphic = Graph;
	SDL_Rect GDims = Parent->GetGraphicDimensions(Graphic);
	IconX = (Target.w - GDims.w) >> 1;

	/* consolidate ? */
	
	if(!DisplaySurface)
		DisplaySurface = OptimalSurface(Target.w, Target.h);
	else
		SDL_FillRect(DisplaySurface, NULL, SDL_MapRGB(DisplaySurface->format, 0, 0, 0));

//	printf("DisplaySurface flags:%08x Aloss:%d Bits:%d Alpha:%d\n", DisplaySurface->flags, DisplaySurface->format->Aloss, DisplaySurface->format->BytesPerPixel, DisplaySurface->format->alpha);

	CGUIMessage Msg;
	Msg.Type = CGUIMessage::DRAW;
	Msg.PositionX = -Target.x; Msg.PositionY = -Target.y;
	Msg.Data.Draw.Target = DisplaySurface;
	if(Over)
		Outline->Message(&Msg);
	Parent->AddGraphic(DisplaySurface, Graphic, IconX, 0);
	if(Text)
		Text->Message(&Msg);
}

bool CGUIFileDlgFile::TypeKnown()
{
	if(Graphic != GFX_UNKNOWN) return true;

	char *UEFExts[] = {"uef\a", NULL};
	return !(GetHost()->ExtensionIncluded(File->Name, UEFExts) && !UEFChecked);
}

void CGUIFileDlgFile::DetermineType(char *path)
{
	/* only happens for UEF, so... */
	char *TBuf = new char[strlen(File->Name) + strlen(path) + 2];
	sprintf(TBuf, "%s%c%s", path, GetHost()->DirectorySeparatorChar(), File->Name);

	gzFile UEF = gzopen(TBuf, "rb");
	UEFChecked = true;
	delete[] TBuf;
	if(UEF)
	{
		bool SnapChunks = false, TapeChunks = false;

		char Intro[10];
		gzread(UEF, Intro, 10);
		if(!strcmp(Intro, "UEF File!"))
		{
			/* looks like a UEF */
			gzseek(UEF, 2, SEEK_CUR); // skip version num

			while(1)
			{
				gzseek(UEF, 1, SEEK_CUR); // skip id low
				Uint8 CIDHigh = gzgetc(UEF); // read id high

				if(gzeof(UEF))
					break;

				if(CIDHigh == 0x1)
				{
					TapeChunks = true;
					break;
				}

				if(CIDHigh == 0x4)
					SnapChunks = true;

				Uint32 Length = gzgetc(UEF);
				Length |= gzgetc(UEF) << 8;
				Length |= gzgetc(UEF) << 16;
				Length |= gzgetc(UEF) << 24;
				gzseek(UEF, Length, SEEK_CUR);
			}
		}
		gzclose(UEF);

		if(TapeChunks)
			SetIcon(GFX_TAPE);
		else
			if(SnapChunks)
				SetIcon(GFX_SNAPSHOT);
	}

	/*
		OLD CODE: slow

	CUEFChunkSelector *sel = GetUEFSelector(TBuf, 0x000a);
	delete[] TBuf;

	UEFChecked = true;

	if(sel)
	{
		if(sel->FindIdMajor(0x01))
			SetIcon(GFX_TAPE);
		else
		{
			if(sel->FindIdMajor(0x04))
				SetIcon(GFX_SNAPSHOT);
		}
		ReleaseUEFSelector(sel);	
	}
	*/
}

class CGUIFileDlgContents: public CGUIObjectCollection
{
	public:
		CGUIFileDlgContents(CGUI &, SDL_Surface *Static, SDL_Rect &FullArea, char *path, FileDesc *FD);
		~CGUIFileDlgContents();
		void SetHolder(CGUIObjectAnalogue *Hlder);

	private:
		FileDesc *FD;
		SDL_Thread *UEFCheckThread;
		volatile bool QuitThread;
		char *CPath;

		int CheckTypes();
		static int CheckTypesHelper(void *tptr);
		CGUIObjectAnalogue *Holder;
};

int CGUIFileDlgContents::CheckTypes()
{
	GUIObjectPtr *P;
	while(!QuitThread)
	{
		SDL_Rect VisArea = Holder->GetVisibleArea();

		/* first: try to find a *P that is unknown type and visible */
		P = Head;
		while(P)
		{
			if(! ((CGUIFileDlgFile *)P->Object)->TypeKnown())
			{
				SDL_Rect CoverArea = P->Object->GetArea();
				if(!
					(
						CoverArea.x > (VisArea.x+VisArea.w) ||
						(CoverArea.x+CoverArea.w) < VisArea.x ||
						CoverArea.y > (VisArea.y+VisArea.h) ||
						(CoverArea.y+CoverArea.h) < VisArea.y
					)
				)
					break;
			}

			P = P->Next;
		}

		/* if not found one yet, try to find any that is simply unknown */
		if(!P)
		{
			P = Head;
			while(P)
			{
				if(! ((CGUIFileDlgFile *)P->Object)->TypeKnown())
					break;

				P = P->Next;
			}
			
		}

		/* if still nothing to work on, then all must know their type - end thread */
		if(!P)
			break;

		((CGUIFileDlgFile *)P->Object)->DetermineType(CPath);

//		c++;
//		if(!(c&15))
//			SDL_Delay(10);
	}
	
	return 0;
}

int CGUIFileDlgContents::CheckTypesHelper(void *tptr)
{
	return ((CGUIFileDlgContents *)tptr)->CheckTypes();
}

CGUIFileDlgContents::~CGUIFileDlgContents()
{
	QuitThread = true;
	SDL_WaitThread(UEFCheckThread, NULL);
	free(CPath);
}

void CGUIFileDlgContents::SetHolder(CGUIObjectAnalogue *Hlder)
{
	Holder = Hlder;
	QuitThread = false;
	UEFCheckThread = SDL_CreateThread(CheckTypesHelper, this);
}

CGUIFileDlgContents::CGUIFileDlgContents(CGUI &Par, SDL_Surface *Static, SDL_Rect &FullArea, char *path, FileDesc *FD)
{
	OffsetX = OffsetY = 0;
	InitialiseList();
	DisplaySurface = NULL;
	CPath = strdup(path);

	SubArea = Target = FullArea;
	SubArea.h = 0;

	/* all supported extensions */
	char *AllExtensions[] = {"csw\a", "uef\a", "adf\a", "adl\a", "adm\a", "dsd\a", "ssd\a", "img\a", "fdi\a", NULL};

	/* get folder contents, generate icons */
	SDL_Rect IconArea = SubArea;
	IconArea.w = 80; IconArea.h = 80;
	FileDesc *FDP = FD;
	bool First = (strlen(CPath) != 1);
	while(FDP)
	{
		if(First || (FDP->Type == FileDesc::FD_DIRECTORY) || GetHost()->ExtensionIncluded(FDP->Name, AllExtensions))
		{
			CGUIFileDlgFile *NewFile = new CGUIFileDlgFile(Par, this, FILE_CLICK, Static, IconArea, First ? NULL : FDP);

			AddObject(NewFile);

			IconArea.x += IconArea.w;
			if(IconArea.x+IconArea.w > SubArea.x+SubArea.w)
			{
				IconArea.x = SubArea.x;
				if(FDP->Next) IconArea.y += IconArea.h;
			}
		}

		if(!First) FDP = FDP->Next;
		First = false;
	}

	SubArea.h = IconArea.y+IconArea.h - SubArea.y;
	Target = SubArea;
}

CGUIFileDlg::CGUIFileDlg(CGUI &Par, SDL_Surface *St, SDL_Rect &FullArea)
{
	OffsetX = OffsetY = 0;
	InitialiseList();
	DisplaySurface = NULL;
	Parent = &Par;
	Static = St;

	/* analogue slider into about and credits display */
	SubArea = Target = FullArea;

	Cont = NULL;
	CurrentPath = NULL;
	FD = NULL;

	SetToPath( Par.GetConfigFile()->ReadString( "StartDirectory", "/" ) );
}

void CGUIFileDlg::SetToPath(char *name)
{
	ClearList();
	if(CurrentPath) free(CurrentPath);

	CurrentPath = strdup(name);
	if(FD) GetHost()->FreeFolderContents(FD);
	FD = GetHost()->GetFolderContents(CurrentPath);
	if(!FD)
	{
		free(CurrentPath);
		CurrentPath = strdup("/");
		FD = GetHost()->GetFolderContents(CurrentPath);
	}

	SDL_Rect Inside = Target;
	Inside.w -= 18;
	Inside.x += 1;
	Cont = new CGUIFileDlgContents(*Parent, NULL, Inside, CurrentPath, FD);
	CGUIObjectAnalogue *MessageArea = new CGUIObjectAnalogue(*Parent, Static, Target, false, Cont);
	Cont->SetHolder(MessageArea);
	AddObject(MessageArea);
}

CGUIFileDlg::~CGUIFileDlg()
{
	if(CurrentPath)
	{
		Parent->GetConfigFile()->WriteString( "StartDirectory", CurrentPath );
		
		free(CurrentPath);
	}
	GetHost()->FreeFolderContents(FD);
}

void CGUIFileDlg::Message(CGUIMessage *Msg)
{
	CGUIObjectCollection::Message(Msg);
	if(Msg->Type == CGUIMessage::EVENT && Msg->Data.Event.Target == Cont)
	{
		/* file click */
		if(!Msg->Data.Event.Source)
		{
			/* reduce path */
			char *TBuf = strdup(CurrentPath), *Parser = TBuf + strlen(TBuf);

			while(*Parser != '\\' && *Parser != '/' && Parser != TBuf)
			{
				Parser--;
			}

			*Parser = '\0';

			if(!strlen(TBuf))
				sprintf(TBuf, "\\");

			SetToPath(TBuf);
			free(TBuf);
		}
		else
			if(((FileDesc *)Msg->Data.Event.Source)->Type == FileDesc::FD_FILE)
			{
				/* open file */
				char *TBuf = new char[strlen(((FileDesc *)Msg->Data.Event.Source)->Name) + strlen(CurrentPath) + 2];
				sprintf(TBuf, "%s%c%s", CurrentPath, GetHost()->DirectorySeparatorChar(), ((FileDesc *)Msg->Data.Event.Source)->Name);
				if(Parent->GetProcessPool()->Open(TBuf))
					Parent->SignalQuit();
				delete TBuf;
			}
			else
			{
				/* append to path */
				char *TBuf = new char[strlen(((FileDesc *)Msg->Data.Event.Source)->Name) + strlen(CurrentPath) + 2];
				char *ITBuf = TBuf;

				sprintf(TBuf, "%s\\%s", CurrentPath, ((FileDesc *)Msg->Data.Event.Source)->Name);
				while(*ITBuf == '\\' || *ITBuf == '/') ITBuf++;

				SetToPath(ITBuf);
				delete[] TBuf;
			}
	}
}

#endif
