/*
 *  WindowsHostMachine.h
 *  ElectrEm
 *
 *  Created by Ewen Roberts on Mon Jul 19 2004.
 *
 */

class WindowsHostMachine : public HostMachine
{
public:
	WindowsHostMachine();
	virtual ~WindowsHostMachine();
	
	// File Wrangling
	virtual FileDesc *GetFolderContents(const char *name);

	// Utility Functions
	virtual void RegisterArgs(int, char *[]);
	virtual void DisplayError(char *fmt, ...);
	virtual void DisplayWarning(char *fmt, ...);
	
	virtual int MaxPathLength() const;
	virtual char DirectorySeparatorChar() const;
	virtual FileSpecs GetSpecs(const char *name);
	virtual void SaveScreenShot( SDL_Surface * surface );

	virtual void FullScreen( bool bFullScreen );

#ifndef HOSTMACHINE_FILEONLY
	// Return a configuration store for the host machine.
	virtual BasicConfigurationStore * OpenConfigurationStore( const char * name );
	virtual void CloseConfigurationStore( BasicConfigurationStore * store );
#endif

#ifndef NO_SDL
	// Allows the host to grab events and respond to them using the process pool.
	// This is intended to be used by external GUIs that want to implement eg their own debugger
	virtual bool ProcessEvent( SDL_Event * evt, CProcessPool * pool );
	virtual void SetupUI( CProcessPool * pool, BasicConfigurationStore * store );
#endif
	

private:
	bool _fullScreen;
	HMENU _newMenu;
	HWND _hMainWnd;
	HWND _hAboutBox;
	HINSTANCE _hInstance;
#ifdef USE_NATIVE_GUI
	PreferencesDialog * _preferences;
#endif
#ifndef HOSTMACHINE_FILEONLY
	BasicConfigurationStore * _configStore;
#endif
	void AboutBox();
	void DropFilesEvent( SDL_Event * evt, CProcessPool * pool );
	void Launch( const char * fileName );
	void Preferences();
	void MenuEvent( SDL_Event * evt, CProcessPool * pool );
	void SetMenu(HMENU m);

protected:
	virtual char *GetExecutablePath();
	
protected:
	char *ExecutablePath;
};

