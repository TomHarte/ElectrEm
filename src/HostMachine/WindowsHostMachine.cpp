/*

  ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator
  
	This is open software, distributed under the GPL 2, see 'Copying' for
	details
	
	  FileHelper.cpp
	  ==============
	  
		Numerous small things that interface between ElectrEm and your OS's
		native file functionality. Most notably contains the filename resolver.
		
*/
#include <windows.h>
#include <direct.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "HostMachine.h"
#include "../ProcessPool.h"
#include "../GUIEvents.h"
#include "../Display.h"

#ifdef USE_NATIVE_GUI
#include "../Configuration/ConfigurationStore.h"
#include "../GUI/Windows/PreferencesDialog.h"
#endif
#include "WindowsHostMachine.h"

#include <windows.h>
#include "SDL_syswm.h"
#include "resource.h"

static WindowsHostMachine hostMachine;

HostMachine * GetHost()
{
	return & hostMachine;
}

WindowsHostMachine::WindowsHostMachine()
{
	_newMenu = NULL;
	_hAboutBox = NULL;
	_hMainWnd = NULL;
#ifdef USE_NATIVE_GUI
	_preferences = NULL;
#endif
	_configStore = NULL;
	_fullScreen = false;
}

WindowsHostMachine::~WindowsHostMachine()
{
	if ( _hAboutBox != NULL )
	{
		::DestroyWindow( _hAboutBox );
	}

#ifdef USE_NATIVE_GUI
	if ( NULL != _preferences )
	{
		delete _preferences;
	}
#endif
}


FileDesc *WindowsHostMachine::GetFolderContents(const char *name)
{
	FileDesc *Head = NULL, **NPtr = &Head;
	if(!strcmp(name, "/") || !strcmp(name, "\\"))
	{
		/* build drive list and return that */
		unsigned long Drives = _getdrives();
		char DriveName[] = "A:";
		
		while(Drives)
		{
			if(Drives&1)
			{
				*NPtr = new FileDesc;
				(*NPtr)->Name = strdup(DriveName);
				(*NPtr)->Type = FileDesc::FD_DIRECTORY;
				(*NPtr)->Next = NULL;
				NPtr = &(*NPtr)->Next;
			}
			
			DriveName[0]++;
			Drives >>= 1;
		}
		
		return Head;
	}
	
	/* win32 thing: we're searching for files, not opening a directory, so... */
	char *SPath;
	if( name[strlen(name)] != '*')
	{
		SPath = new char[strlen(name)+3];
		strcpy(SPath, name);
		
		if( SPath[strlen(SPath)] != '\\')
		{
			SPath[strlen(SPath)+1] = '\0';
			SPath[strlen(SPath)] = '\\';
		}
		
		if( SPath[strlen(SPath)] != '*')
		{
			SPath[strlen(SPath)+1] = '\0';
			SPath[strlen(SPath)] = '*';
		}
	}
	else
	{
		SPath = new char[strlen(name)+1];
		strcpy(SPath, name);
	}
	
	WIN32_FIND_DATA FoundFile;
	HANDLE DirHandle;
	
	if( (DirHandle = FindFirstFile(SPath, &FoundFile)) != INVALID_HANDLE_VALUE)
	{
		do
		{
			if(!(FoundFile.dwFileAttributes&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)) &&
				strcmp(FoundFile.cFileName, "..") && strcmp(FoundFile.cFileName, "."))
			{
				*NPtr = new FileDesc;
				(*NPtr)->Name = strdup(FoundFile.cFileName);
				(*NPtr)->Type = (FoundFile.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) ? FileDesc::FD_DIRECTORY : FileDesc::FD_FILE;
				(*NPtr)->Next = NULL;
				NPtr = &(*NPtr)->Next;
			}
		}
		while(FindNextFile(DirHandle, &FoundFile));
		
		FindClose(DirHandle);
	}
	
	delete[] SPath;
	
	return SortFDList(Head);
}

// Utility Functions
void WindowsHostMachine::RegisterArgs(int, char *[])
{
}

void WindowsHostMachine::DisplayError(char *fmt, ...)
{
	char Message[512];
	va_list Arguments;
	
	va_start(Arguments, fmt);
	vsprintf(Message, fmt, Arguments);
	va_end(Arguments);
	
	MessageBox(NULL, Message, "ElectrEm: Fatal Error", MB_OK | MB_ICONEXCLAMATION);
}

void WindowsHostMachine::DisplayWarning(char *fmt, ...)
{
	char Message[512];
	va_list Arguments;
	
	va_start(Arguments, fmt);
	vsprintf(Message, fmt, Arguments);
	va_end(Arguments);
	
	MessageBox(NULL, Message, "ElectrEm: Warning", MB_OK | MB_ICONINFORMATION);
}

int WindowsHostMachine::MaxPathLength() const
{
	return MAX_PATH;
}

char WindowsHostMachine::DirectorySeparatorChar() const
{
	return '\\';
}

char *WindowsHostMachine::GetExecutablePath()
{
	char *NewBuf = new char[MAX_PATH];
	GetModuleFileName(NULL, NewBuf, MAX_PATH);
	
	char *IPtr = NewBuf + strlen(NewBuf);
	while(*IPtr != '\\') IPtr--;
	*IPtr = '\0';
	return NewBuf;
}

#ifndef NO_SDL
BasicConfigurationStore * WindowsHostMachine::OpenConfigurationStore( const char * name )
{
	if ( NULL != _configStore )
	{
		delete _configStore;
	}
	_configStore = new ConfigurationStore( name );
	return _configStore;
}

void WindowsHostMachine::CloseConfigurationStore( BasicConfigurationStore * store )
{
	// Deselect the config store that is active
	if ( store == _configStore )
	{
		_configStore = NULL;
	}

	// delete the store that was passed in
	if ( NULL != store )
	{
		delete store;
	}
}

#endif

#ifndef NO_SDL


// Allows the host to grab events and respond to them using the process pool.
// This is intended to be used by external GUIs that want to implement eg their own debugger
// Win32 specific WM messages - i.e. drag and drop, maximise button
bool WindowsHostMachine::ProcessEvent( SDL_Event * evt, CProcessPool * pool )
{
	switch ( evt->type )
	{
	case SDL_SYSWMEVENT:
		switch(evt->syswm.msg->msg)
		{
		case WM_DROPFILES:
			DropFilesEvent( evt, pool );
			break;
		case WM_COMMAND:
			MenuEvent( evt, pool );
			break;
		}
		break;
	}
	return HostMachine::ProcessEvent( evt, pool );
}

// Called when user drops files from explorer onto our main window, loads the files into the emulator
void WindowsHostMachine::DropFilesEvent( SDL_Event * evt, CProcessPool * pool)
{
	HDROP hDrop = (HDROP)evt->syswm.msg->wParam;
	
	int NumFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
	while(NumFiles--)
	{
		int BufSize = DragQueryFile(hDrop, NumFiles, NULL, 0) + 1;
		char *TextBuffer = new char[BufSize];
		
		DragQueryFile(hDrop, NumFiles, TextBuffer, BufSize);
//		pool->IOCtl(PPOOLIOCTL_OPEN, strdup(TextBuffer));
		if(!pool->Open(TextBuffer))
			GetHost() -> DisplayError("Unable to open %s\n", TextBuffer);
		delete[] TextBuffer;
	}
	
	DragFinish(hDrop);
}

// Handles a menu event in the main window
void WindowsHostMachine::MenuEvent( SDL_Event * evt, CProcessPool * pool )
{
	switch ( LOWORD( evt->syswm.msg->wParam ) )
	{
	case ID_OPEN_FILE:
		{
			char fileName[MAX_PATH ];
			::ZeroMemory( fileName, MAX_PATH );
			OPENFILENAME ofn;
			::ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
			ofn.lStructSize = sizeof( OPENFILENAME );
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = "All compatible files\0*.uef;*.adf;*.adl;*.adm;*.ssd;*.dsd;*.img;*.csw;*.bas;*.bbc;*.txt\0Tape Images\0*.uef;*.csw\0Disc Images\0*.adf;*.adl;*.adm;*.ssd;*.dsd;*.img\0BASIC Source Files\0*.bas;*.bbc;*.txt\0";
			
			if ( GetOpenFileName( &ofn ) )
			{
				SDL_Event sdlevt;
				sdlevt.type = SDL_USEREVENT;
				sdlevt.user.code = GUIEVT_LOADFILE;
				sdlevt.user.data1 = strdup( ofn.lpstrFile );
				SDL_PushEvent( &sdlevt ); 
			}
		}
		break;

	case ID_TAPE_REWIND:
	{
		SDL_Event sdlevt;
		sdlevt.type = SDL_USEREVENT;
		sdlevt.user.code = GUIEVT_REWINDTAPE;
		SDL_PushEvent( &sdlevt ); 
	}
	break;

	case ID_TAPE_EJECT:
	{
		SDL_Event sdlevt;
		sdlevt.type = SDL_USEREVENT;
		sdlevt.user.code = GUIEVT_EJECTTAPE;
		SDL_PushEvent( &sdlevt ); 
	}
	break;

	case ID_TAPE_INSERT:
	{
			char fileName[MAX_PATH ];
			::ZeroMemory( fileName, MAX_PATH );
			OPENFILENAME ofn;
			::ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
			ofn.lStructSize = sizeof( OPENFILENAME );
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = "Tape Images\0*.uef;*.csw\0";
			
			if ( GetOpenFileName( &ofn ) )
			{
				SDL_Event sdlevt;
				sdlevt.type = SDL_USEREVENT;
				sdlevt.user.code = GUIEVT_INSERTTAPE;
				sdlevt.user.data1 = strdup( ofn.lpstrFile );
				SDL_PushEvent( &sdlevt ); 
			}
	}
	break;

	case ID_DISC_INSERT0:
	case ID_DISC_INSERT1:
	{
			char fileName[MAX_PATH ];
			::ZeroMemory( fileName, MAX_PATH );
			OPENFILENAME ofn;
			::ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
			ofn.lStructSize = sizeof( OPENFILENAME );
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = "Disc Images\0*.adf;*.adl;*.adm;*.ssd;*.dsd;*.img\0";
			
			if ( GetOpenFileName( &ofn ) )
			{
				SDL_Event sdlevt;
				sdlevt.type = SDL_USEREVENT;
				sdlevt.user.code = (LOWORD( evt->syswm.msg->wParam ) == ID_DISC_INSERT0) ? GUIEVT_INSERTDISC0 : GUIEVT_INSERTDISC1;
				sdlevt.user.data1 = strdup( ofn.lpstrFile );
				SDL_PushEvent( &sdlevt ); 
			}
	}
	break;

	case ID_DISC_EJECT0:
	case ID_DISC_EJECT1:
	{
		SDL_Event sdlevt;
		sdlevt.type = SDL_USEREVENT;
		sdlevt.user.code = (LOWORD( evt->syswm.msg->wParam ) == ID_DISC_EJECT0) ? GUIEVT_EJECTDISC0 : GUIEVT_EJECTDISC1;
		SDL_PushEvent( &sdlevt ); 
	}
	break;

	case ID_BASIC_IMPORT:
		{
			char fileName[MAX_PATH ];
			::ZeroMemory( fileName, MAX_PATH );
			OPENFILENAME ofn;
			::ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
			ofn.lStructSize = sizeof( OPENFILENAME );
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = "Plain text format\0*.txt;*.bas;*.bbc\0All files\0*\0";
			
			if ( GetOpenFileName( &ofn ) )
			{
				SDL_Event sdlevt;
				sdlevt.type = SDL_USEREVENT;
				sdlevt.user.code = GUIEVT_IMPORTBASIC;
				sdlevt.user.data1 = strdup( ofn.lpstrFile );
				SDL_PushEvent( &sdlevt ); 
			}
		}
		break;

	case ID_SAVE_STATE:
		{
			char fileName[MAX_PATH ];
			::ZeroMemory( fileName, MAX_PATH );
			OPENFILENAME ofn;
			::ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
			ofn.lStructSize = sizeof( OPENFILENAME );
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = "UEF\0*.uef\0";

			if ( GetSaveFileName( &ofn ) )
			{
				SDL_Event sdlevt;
				sdlevt.type = SDL_USEREVENT;
				sdlevt.user.code = GUIEVT_SAVESTATE;
				sdlevt.user.data1 = strdup( ofn.lpstrFile );
				SDL_PushEvent( &sdlevt ); 
			}
		}
		break;

	case ID_SET_PRINTER:
		{
			char fileName[MAX_PATH ];
			::ZeroMemory( fileName, MAX_PATH );
			OPENFILENAME ofn;
			::ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
			ofn.lStructSize = sizeof( OPENFILENAME );
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = "Rich Text Format\0*.rtf\0Plain Text Format\0*.txt\0";
	
			if ( GetSaveFileName( &ofn ) )
			{
				char *NamePtr = ofn.lpstrFile;
				char TempString[2048];

				//append file extension, if user didn't type it. Windows is a pain
				switch(ofn.nFilterIndex)
				{
					default: break;
					case 1: //check for/append .rtf
					{
						char *RTFExts[] = { "rtf", NULL};

						if(! GetHost()->ExtensionIncluded(NamePtr, RTFExts))
						{
							sprintf(TempString, "%s.rtf", NamePtr);
							NamePtr = TempString;
						}
					}
					break;
					case 2: //check for/append .txt
					{
						char *TXTExts[] = { "txt", NULL};

						if(! GetHost()->ExtensionIncluded(NamePtr, TXTExts))
						{
							sprintf(TempString, "%s.txt", NamePtr);
							NamePtr = TempString;
						}
					}
					break;
				}

				SDL_Event sdlevt;
				sdlevt.type = SDL_USEREVENT;
				sdlevt.user.code = GUIEVT_SETPRINTERFILE;
				sdlevt.user.data1 = strdup( NamePtr);
				SDL_PushEvent( &sdlevt ); 
			}
		}
		break;

	case ID_BASIC_EXPORT:
		{
			char fileName[MAX_PATH ];
			::ZeroMemory( fileName, MAX_PATH );
			OPENFILENAME ofn;
			::ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
			ofn.lStructSize = sizeof( OPENFILENAME );
			ofn.lpstrFile = fileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = "Plain Text Format\0*.txt;*.bas;*.bbc\0";
			
			if ( GetSaveFileName( &ofn ) )
			{
				SDL_Event sdlevt;
				sdlevt.type = SDL_USEREVENT;
				sdlevt.user.code = GUIEVT_EXPORTBASIC;
				sdlevt.user.data1 = strdup( ofn.lpstrFile );
				SDL_PushEvent( &sdlevt ); 
			}
		}
		break;

	case ID_FILE_EXIT:
		{
			SDL_Event sdlevt;
			sdlevt.type = SDL_USEREVENT;
			sdlevt.user.code = PPDEBUG_USERQUIT;
			SDL_PushEvent( &sdlevt );
		}
		break;

	case ID_COPY_SCREEN:
		{
			SDL_Event sdlevt;
			sdlevt.type = SDL_USEREVENT;
			sdlevt.user.code = GUIEVT_COPY;
			SDL_PushEvent( &sdlevt );
		}
		break;

	case ID_PASTE_TEXT:
		{
			/* get text from clipboard... */
			char *buffer;
			if(OpenClipboard(NULL))
			{

				if(buffer = (char*)GetClipboardData(CF_TEXT))
				{
					SDL_Event sdlevt;
					sdlevt.type = SDL_USEREVENT;
					sdlevt.user.code = GUIEVT_INSERTTEXT;
					sdlevt.user.data1 = strdup( buffer );
					SDL_PushEvent( &sdlevt );
				}
			}

			CloseClipboard();
		}
		break;

	case ID_TOOLS_FULLSCREEN:
		{
			SDL_Event sdlevt;
			sdlevt.type = SDL_USEREVENT;
			sdlevt.user.code = PPDEBUG_FSTOGGLE;
			SDL_PushEvent( &sdlevt );
		}
		break;

	case ID_TOOLS_PREFERENCES:
		{
			Preferences();
		}
		break;

	case ID_TOOLS_RESET:
		{
			SDL_Event sdlevt;
			sdlevt.type = SDL_USEREVENT;
			sdlevt.user.code = GUIEVT_RESET;
			SDL_PushEvent( &sdlevt );
		}
		break;

	case ID_HELP_ABOUT:
		AboutBox();
		break;

	case ID_HELP_README:
		Launch( "Readme.rtf" );
		break;

	case ID_HELP_ELECTREMWEBSITE:
		Launch( "http://electrem.acornelectron.co.uk/" );
		break;

	case ID_HELP_TECHNICALNOTES:
		Launch( "technotes.html" );
		break;

	}
}

// DialogProc for the about dialog, handles dialog events
BOOL CALLBACK AboutDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
	switch (message) 
	{ 
	case WM_COMMAND:
		{
			if ( LOWORD( wParam ) == IDOK )
			{
				::ShowWindow( hwndDlg, SW_HIDE );
				return TRUE;
			}
		}
		break;
	}
	return FALSE;
}

// Called to open the about dialog. If the dialog has not been shown, create it. Otherwise just show it.
void WindowsHostMachine::AboutBox()
{
	if ( NULL == _hAboutBox )
	{
		_hAboutBox = ::CreateDialog( _hInstance, MAKEINTRESOURCE( IDD_ABOUTDIALOG ), _hMainWnd, AboutDlgProc );
		if ( NULL == _hAboutBox )
		{
			MessageBox( _hMainWnd, "Failed to make about dialog", "ElectrEm", MB_OK );
			return;
		}

	}
	::ShowWindow( _hAboutBox, SW_SHOW );
}

// My version of SetMenu, ensures the ClientRect remains the same size
void WindowsHostMachine::SetMenu(HMENU m)
{
	RECT ClientArea;
	GetClientRect( _hMainWnd, &ClientArea);

	::SetMenu( _hMainWnd, m);

	RECT NewClientArea;
	GetClientRect( _hMainWnd, &NewClientArea);

	/* check if we've lost pixels */
	if(NewClientArea.bottom != ClientArea.bottom)
	{
		/* fix up */
		RECT WindowArea;
		GetWindowRect( _hMainWnd, &WindowArea);
		WindowArea.bottom += ClientArea.bottom - NewClientArea.bottom;

		MoveWindow( _hMainWnd, WindowArea.left, WindowArea.top, WindowArea.right - WindowArea.left, WindowArea.bottom - WindowArea.top, TRUE);
	}
}

void WindowsHostMachine::FullScreen( bool bFullScreen )
{
	_fullScreen = bFullScreen;
	if ( bFullScreen )
	{
		SetMenu( NULL );
	}
	else
	{
		SetMenu( _newMenu );
	}
}

// Launch a file (equivalent to double clicking it)
void WindowsHostMachine::Launch( const char * fileName )
{
	::ShellExecute( _hMainWnd, "open", fileName, NULL, "" , SW_SHOW );
}

// Open the preferences dialog
void WindowsHostMachine::Preferences()
{
#ifdef USE_NATIVE_GUI
	if ( NULL == _preferences )
	{
		_preferences = new PreferencesDialog( _hInstance, _hMainWnd, _configStore );
//		_preferences->InitialisePreferenceDialog( _hMainWnd);
	}
	_preferences -> Show();
#endif
}

// Set up the windows ui by manipulating the main window to add a menu
void WindowsHostMachine::SetupUI( CProcessPool * pool, BasicConfigurationStore * store )
{
#ifdef USE_NATIVE_GUI
	/* if win32, enable DROPFILES message */
	SDL_SysWMinfo SystemInfo;
	SDL_VERSION(&SystemInfo.version);
	if(SDL_GetWMInfo(&SystemInfo) == 1)
	{
		_hMainWnd = SystemInfo.window;
		_hInstance = ::GetModuleHandle(NULL);

		LONG ExStyle = GetWindowLong( _hMainWnd, GWL_EXSTYLE);
		//		LONG Style = GetWindowLong(SystemInfo.window, GWL_STYLE);
		
		ExStyle |= WS_EX_ACCEPTFILES;
		//		Style |= WS_MAXIMIZEBOX;
		
		SetWindowLong( _hMainWnd, GWL_EXSTYLE, ExStyle);
		//		SetWindowLong(SystemInfo.window, GWL_STYLE, Style);
	
		HICON icon = ::LoadIcon( _hInstance, MAKEINTRESOURCE( IDI_ICONELECTREM ) );
		::SendMessage( _hMainWnd, WM_SETICON, TRUE, (LPARAM)icon );
		::SendMessage( _hMainWnd, WM_SETICON, FALSE, (LPARAM)icon );

		_newMenu = ::LoadMenu( _hInstance, MAKEINTRESOURCE( IDR_EMUMENU ) );

		_fullScreen = ((CDisplay *)pool->GetWellDefinedComponent(COMPONENT_DISPLAY))->QueryFullScreen();
		if ( !_fullScreen )
		{
			SetMenu( _newMenu );
		}

		SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
	}

	HostMachine::SetupUI( pool, store );
#endif
}

#endif

//
// Place the screen surface onto the windows clipboard.
//
void WindowsHostMachine::SaveScreenShot( SDL_Surface * surface )
{
	// We have a 24 bit surface which we will convert to a DIB for storage on the clipboard
	int bmpRowSize = surface -> w * 3;
	int bmpSize = sizeof( BITMAPINFO ) + ( bmpRowSize * surface -> h );
	HANDLE dib = ::GlobalAlloc( GMEM_MOVEABLE, bmpSize );
	BITMAPINFO * bmp = (BITMAPINFO *)::GlobalLock( dib );
	::ZeroMemory( bmp, bmpSize );
	bmp -> bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
	bmp -> bmiHeader.biWidth = surface -> w;
	bmp -> bmiHeader.biHeight = surface -> h;
	bmp -> bmiHeader.biPlanes = 1;
	bmp -> bmiHeader.biBitCount = 24;
	bmp -> bmiHeader.biCompression = BI_RGB;
	bmp -> bmiHeader.biSizeImage = 0;
	
	char * bmpRow = ((char *)bmp) + sizeof( BITMAPINFOHEADER );
	// Just to be difficult windows DIBs are stored with the pixels in inverse order.
	for ( int row = ( surface -> h - 1 ); row > -1; row -- )
	{
		memcpy( bmpRow, ((char*)surface -> pixels ) + ( row * bmpRowSize ), bmpRowSize );
		bmpRow += bmpRowSize;
	}
	
	::GlobalUnlock( dib );
	
	// This is the lazy way, we're using the old-style clipboard that dates back to win16!
	::OpenClipboard( NULL);
	::SetClipboardData( CF_DIB, dib );
	::CloseClipboard();
	// The DIB memory we allocated now belongs to windows, we're not supposed to free it.
	
	// Free the surface though...
	HostMachine::SaveScreenShot( surface );
}

#include <sys/stat.h>

FileSpecs WindowsHostMachine::GetSpecs(const char *name)
{
	FileSpecs spec;
	
	struct _stat fstats;
	_stat(name, &fstats);
	
	spec.Size = fstats.st_size;
	spec.Stats = (fstats.st_mode&_S_IWRITE) ? 0 : FS_READONLY;
	
	return spec;
}
