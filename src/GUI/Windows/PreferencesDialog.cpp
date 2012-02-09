#ifdef USE_NATIVE_GUI

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "commctrl.h"
#include <SDL.h>
#include "../../GUIEvents.h"
#include <stdlib.h>

#ifndef LPCBYTE
#define LPCBYTE CONST BYTE *
#endif

#include "resource.h"
#include "../../Configuration/ConfigurationStore.h"
#include "../../Configuration/ElectronConfiguration.h"
#include "preferencesdialog.h"

static PreferencesDialog * s_currentDialog = NULL;

// DialogProc for the preferences dialog, handles dialog events
BOOL CALLBACK PrefsDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
	if ( NULL == s_currentDialog )
	{
		return FALSE;
	}
	switch (message) 
	{ 
	case WM_INITDIALOG:
		s_currentDialog -> InitialisePreferenceDialog( hwndDlg );
		break;

	case WM_COMMAND:
		{
			if ( LOWORD( wParam ) == IDOK )
			{
				s_currentDialog -> UpdatePreferences();
				s_currentDialog -> Hide();
				return TRUE;
			}
			else if ( LOWORD( wParam ) == IDCANCEL )
			{
				s_currentDialog -> Hide();
				return TRUE;
			}
		}
	case WM_NOTIFY:
		{
			if (  IDC_CHECKDISK == wParam || IDC_CHECKSLOGGER == wParam )
			{
				s_currentDialog -> RefreshChecks();
			}
		}
		break;
	}
	return FALSE;
}

PreferencesDialog::PreferencesDialog( HINSTANCE hInstance, HWND hMainWnd,  BasicConfigurationStore * store )
{
	_hPreferences = NULL;
	_store = store;
	s_currentDialog = this;
	_hPreferences = ::CreateDialog( hInstance, MAKEINTRESOURCE( IDD_PREFSDIALOG ), hMainWnd, PrefsDlgProc );
	if ( NULL == _hPreferences )
	{
		MessageBox( hMainWnd, "Failed to make about dialog", "ElectrEm", MB_OK );
		return;
	}
	Show();
}

PreferencesDialog::~PreferencesDialog(void)
{
	::DestroyWindow( _hPreferences );
	s_currentDialog = NULL;
}

void PreferencesDialog::Disable( UINT ctrlID, bool disable )
{
	HWND hCtrl = ::GetDlgItem( _hPreferences, ctrlID );
	::EnableWindow( hCtrl, (disable) ? 0 : 1 );
}

void PreferencesDialog::Hide(  )
{
	::ShowWindow( _hPreferences, SW_HIDE );
}


void PreferencesDialog::InitialisePreferenceDialog( HWND hDlg )
{
	if ( NULL == _hPreferences )
	{
		_hPreferences = hDlg;
	}
	RefreshPreferences();
}

void PreferencesDialog::RefreshPreferences()
{
	ElectronConfiguration config;
	config.Read( _store );
	SetCheck( IDC_CHECKAUTOLOAD, config.Autoload );
	SetCheck( IDC_CHECKAUTOCONFIGURE, config.Autoconfigure );
	SetCheck( IDC_CHECKFIRSTBYTE, config.FirstByte );
	SetCheck( IDC_CHECKFASTLOAD, config.FastTape );
	SetCheck( IDC_CHECKMULTIPLEX, config.Display.DisplayMultiplexed );
	SetCheck( IDC_CHECKFULLSCREEN, config.Display.StartFullScreen );
	SetCheck( IDC_CHECKOVERLAY, config.Display.AllowOverlay );
	SetCheck( IDC_CHECKDISK, config.Plus3.Enabled  );
	SetCheck( IDC_CHECKPROTECT0, config.Plus3.Drive1WriteProtect  );
	SetCheck( IDC_CHECKPROTECT1, config.Plus3.Drive2WriteProtect  );
	SetCheck( IDC_CHECKSLOGGER, (config.MRBMode != 0) );
	SetCheck( IDC_RADIOTURBO, (config.MRBMode < 2) );
	SetCheck( IDC_RADIOSHADOW, (config.MRBMode == 2) );
	SetCheck( IDC_RADIO4, (config.MRBMode > 2) );
	SetCheck( IDC_CHECKUEFFILES, FileTypeRegistered( ".uef", "ueffile" ) );
	SetCheck( IDC_CHECKADFFILES, FileTypeRegistered( ".adf", "adffile" ) );
	SetCheck( IDC_CHECKSSDFILES, FileTypeRegistered( ".ssd", "ssdfile" ) );
	SetCheck( IDC_CHECKBASICFILES, FileTypeRegistered( ".bas", "basfile" ) );
	SetCheck( IDC_CHECKPLUS1, config.Plus1 );
	SetCheck( IDC_CHECKSAVESTATE, config.PersistentState);

	HWND hCtrl = ::GetDlgItem( _hPreferences, IDC_SLIDERVOLUME );
	::SendMessage( hCtrl, TBM_SETRANGE, FALSE, MAKELONG(0, 255));
	SetSlider( IDC_SLIDERVOLUME, config.Volume);

	RefreshChecks();
}

void PreferencesDialog::RefreshChecks()
{
	bool haveDisk = GetCheck( IDC_CHECKDISK );
	Disable( IDC_CHECKPROTECT0, !haveDisk );
	Disable( IDC_CHECKPROTECT1, !haveDisk );

	bool haveSlogger = GetCheck( IDC_CHECKSLOGGER );
	Disable( IDC_RADIOTURBO, !haveSlogger );
	Disable( IDC_RADIOSHADOW, !haveSlogger );
	Disable( IDC_RADIO4, !haveSlogger );
}

void PreferencesDialog::Show(  )
{
	RefreshPreferences();
	::ShowWindow( _hPreferences, SW_SHOW );
}

bool PreferencesDialog::GetCheck( UINT checkID )
{
	HWND hCtrl = ::GetDlgItem( _hPreferences, checkID );
	return ( 1 == ::SendMessage( hCtrl, BM_GETCHECK, 0, 0 ) );
}

void PreferencesDialog::SetCheck( UINT checkID, bool value )
{
	HWND hCtrl = ::GetDlgItem( _hPreferences, checkID );
	::SendMessage( hCtrl, BM_SETCHECK, (value) ? BST_CHECKED : BST_UNCHECKED, 0 ); 
}

int PreferencesDialog::GetSlider( UINT checkID )
{
	HWND hCtrl = ::GetDlgItem( _hPreferences, checkID );
	return SendMessage( hCtrl, TBM_GETPOS, 0, 0);
}

void PreferencesDialog::SetSlider( UINT checkID, int value )
{
	HWND hCtrl = ::GetDlgItem( _hPreferences, checkID );
	::SendMessage( hCtrl, TBM_SETPOS, TRUE, value);
}

void PreferencesDialog::UpdatePreferences()
{
	ElectronConfiguration *config = new ElectronConfiguration;
	config->Autoload = GetCheck( IDC_CHECKAUTOLOAD );
	config->Autoconfigure = GetCheck( IDC_CHECKAUTOCONFIGURE );
	config->Plus1 = GetCheck( IDC_CHECKPLUS1 );
	config->FirstByte = GetCheck( IDC_CHECKFIRSTBYTE );
	config->FastTape = GetCheck( IDC_CHECKFASTLOAD );
	config->Display.DisplayMultiplexed = GetCheck( IDC_CHECKMULTIPLEX );
	config->Display.StartFullScreen = GetCheck( IDC_CHECKFULLSCREEN );
	config->Display.AllowOverlay = GetCheck( IDC_CHECKOVERLAY );
	config->Plus3.Enabled  = GetCheck( IDC_CHECKDISK );
	config->Plus3.Drive1WriteProtect  = GetCheck( IDC_CHECKPROTECT0 );
	config->Plus3.Drive2WriteProtect  = GetCheck( IDC_CHECKPROTECT1 );
	config->Volume = GetSlider( IDC_SLIDERVOLUME );
	config->PersistentState = GetCheck( IDC_CHECKSAVESTATE );

	if ( GetCheck( IDC_CHECKSLOGGER ) )
	{
		if ( GetCheck( IDC_RADIO4 ) )
		{
			config->MRBMode = MRB_4Mhz;
		}
		else if ( GetCheck( IDC_RADIOTURBO ) )
		{
			config->MRBMode = MRB_TURBO;
		}
		else
		{
			config->MRBMode = MRB_SHADOW;
		}
	}
	else
	{
		config->MRBMode = MRB_OFF;
	}

	config->Write( _store );

	// Update the registry to set the file types
	if ( GetCheck( IDC_CHECKUEFFILES ) )
	{
		CreateFileType( ".uef", "ueffile", "Universal Emulator Format File", 1 );
	}
	else
	{
		DeleteFileType( ".uef", "ueffile" );
	}

	if ( GetCheck( IDC_CHECKADFFILES ) )
	{
		CreateFileType( ".adf", "adffile", "Acorn ADFS Disk Image", 2 );
	}
	else
	{
		DeleteFileType( ".adf", "adffile" );
	}

	if ( GetCheck( IDC_CHECKSSDFILES ) )
	{
		CreateFileType( ".ssd", "ssdfile", "Acorn DFS Single Sided Disk Image", 3 );
	}
	else
	{
		DeleteFileType( ".ssd", "ssdfile" );
	}

	if ( GetCheck( IDC_CHECKBASICFILES ) )
	{
		CreateFileType( ".bas", "basfile", "BBC BASIC source code", 3 );
	}
	else
	{
		DeleteFileType( ".bas", "basfile" );
	}

	/* pass to emulator */
    SDL_Event evt;
	evt.type = SDL_USEREVENT;
	evt.user.code = GUIEVT_ASSIGNCONFIG;
	evt.user.data1 = config;
	/* may have to wait here if event queue is full */
	while( SDL_PushEvent( &evt ) == -1)
		SDL_Delay(10);
}


void PreferencesDialog::CreateFileType( LPCTSTR fileExt, LPCTSTR extGrp, LPCTSTR typeName, int iconNum )
{
	DWORD dwIgnore = 0;
	TCHAR exeFileName[ MAX_PATH ],
				fileNameBuffer[ MAX_PATH ];
	HKEY hExtKey = NULL;
	HKEY hGrpKey = NULL;
	HKEY hIconKey = NULL;
	HKEY hExeKey = NULL;

	::GetModuleFileName( NULL, exeFileName, MAX_PATH );

	// Create a key which maps the file extension to the type
	if ( ERROR_SUCCESS != ::RegCreateKeyEx( HKEY_CLASSES_ROOT, fileExt, 0,  NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hExtKey, &dwIgnore  ) )
	{
		return;
	}
	::RegSetValueEx( hExtKey, "", 0, REG_SZ, (LPCBYTE)extGrp, lstrlen( extGrp ) + 1 );

	::RegCloseKey( hExtKey );
	// Create the file type info key
	dwIgnore = 0;
	if ( ERROR_SUCCESS != ::RegCreateKeyEx( HKEY_CLASSES_ROOT, extGrp, 0,  NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hGrpKey, &dwIgnore  ) )
	{
		// Failed! roll back the extension key
		::RegDeleteKey( HKEY_CLASSES_ROOT, fileExt );
		return;
	}
	::RegSetValueEx( hGrpKey, "", 0, REG_SZ, (LPCBYTE)typeName, lstrlen( typeName ) + 1 );

	// Create a key in the file type info to specify the icon
	dwIgnore = 0;
	if ( ERROR_SUCCESS != ::RegCreateKeyEx( hGrpKey, "DefaultIcon", 0,  NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hIconKey, &dwIgnore  ) )
	{
		// Failed! roll back the extension key
		::RegDeleteKey( HKEY_CLASSES_ROOT, fileExt );
		::RegCloseKey( hGrpKey );
		::RegDeleteKey( HKEY_CLASSES_ROOT, extGrp );
		return;
	}

	// Create a key in the file type info to specify the open action
	wsprintf( fileNameBuffer, "%s,%d", exeFileName, iconNum );
	::RegSetValueEx( hIconKey, "", 0, REG_SZ, (LPCBYTE)fileNameBuffer, lstrlen( fileNameBuffer ) + 1 );
	::RegCloseKey( hIconKey );
	hIconKey = NULL;

	if ( ERROR_SUCCESS != RegCreateKeyEx( hGrpKey, "Shell\\Open\\Command", 0,  NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hExeKey, &dwIgnore  ) )
	{
		// Failed! roll back the extension key
		::RegDeleteKey( HKEY_CLASSES_ROOT, fileExt );
		::RegCloseKey( hGrpKey );
		::RegDeleteKey( HKEY_CLASSES_ROOT, extGrp );
	}

	wsprintf( fileNameBuffer, "\"%s\" \"%%1\"", exeFileName, iconNum );
	::RegSetValueEx( hExeKey, "", 0, REG_SZ, (LPCBYTE)fileNameBuffer, lstrlen( fileNameBuffer ) + 1 );

	// Success! close the keys we created.
	::RegCloseKey( hExeKey );
	::RegCloseKey( hGrpKey );
}

void PreferencesDialog::DeleteFileType( LPCTSTR fileExt, LPCTSTR extGrp )
{
	if ( !FileTypeRegistered( fileExt, extGrp ) )
	{
		return;
	}

	::RegDeleteKey( HKEY_CLASSES_ROOT, fileExt );

	HKEY hGrp;
	if (  ERROR_SUCCESS != ::RegOpenKeyEx( HKEY_CLASSES_ROOT, extGrp, 0, KEY_READ, &hGrp ) )
	{
		return;
	}

	::RegDeleteKey( hGrp, "Shell\\Open\\Command" );
	::RegDeleteKey( hGrp, "Shell\\Open" );
	::RegDeleteKey( hGrp, "Shell" );
	::RegDeleteKey( hGrp, "DefaultIcon" );
	::RegCloseKey( hGrp );

	::RegDeleteKey( HKEY_CLASSES_ROOT, extGrp );
}

bool PreferencesDialog::FileTypeRegistered ( LPCTSTR fileExt, LPCTSTR extGrp )
{
	HKEY hExt;
	HKEY hGrp;
	HKEY hOpen;

	if ( ERROR_SUCCESS != ::RegOpenKeyEx( HKEY_CLASSES_ROOT, fileExt, 0, KEY_READ, &hExt ) )
	{
		return false;
	}

	::RegCloseKey( hExt );

	if (  ERROR_SUCCESS != ::RegOpenKeyEx( HKEY_CLASSES_ROOT, extGrp, 0, KEY_READ, &hGrp ) )
	{
		return false;
	}

	TCHAR fileNameBuffer[ MAX_PATH ];
	TCHAR exeFileName[ MAX_PATH ];
	DWORD size = MAX_PATH;
	::GetModuleFileName( NULL, exeFileName, MAX_PATH );

	if (  ERROR_SUCCESS != ::RegOpenKeyEx( hGrp, "Shell\\Open\\Command", 0, KEY_READ, &hOpen ) )
	{
		::RegCloseKey( hGrp );
		return false;
	}

	if ( ERROR_SUCCESS != ::RegQueryValueEx( hOpen, "", 0, NULL, (LPBYTE)fileNameBuffer, &size ) )
	{
		::RegCloseKey( hOpen );
		::RegCloseKey( hGrp );
		return false;
	}

	::RegCloseKey( hOpen );
	::RegCloseKey( hGrp );

	if ( NULL == strstr( fileNameBuffer,  exeFileName ) )
	{
		return false;
	}

	return true;
}

#endif
