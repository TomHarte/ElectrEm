/*

	ElectrEm (c) 2000-6 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	Main.cpp
	========

	Launching point for the whole emulator. Loads configuration, launches
	machine, closes it if the user issues a quit command and saves the
	configuration at the end

*/

#include "HostMachine/HostMachine.h"
#include "ProcessPool.h"
#include "ComponentBase.h"
#include "SDL.h"
#if !defined(USE_NATIVE_GUI) && !defined(NO_GUI)
	#include "GUI/GUI.h"
#endif
#include "ULA.h"
#include "Plus1/Plus1.h"
#include "Display.h"
#include "GUIEvents.h"
#include "BASIC.h"
#include "6502.h"
#include "Plus3/WD1770.h"
#include "Tape/Tape.h"

/* the next two are to make sure unlink is defined */
#ifdef POSIX
#include <unistd.h>
#endif

#ifdef WIN32
#include <io.h>
#endif


#include <string.h>
#include <malloc.h>

#define GetBasicMemBase(v)\
	ElectronConfiguration CConfig; PPool->IOCtl(IOCTL_GETCONFIG, &CConfig);\
	if(CConfig.MRBMode != MRB_SHADOW)\
		v = 0;\
	else\
	{\
		CULA *ula = (CULA *)PPool->GetWellDefinedComponent(COMPONENT_ULA);\
		v = ula->GetMappedAddr(MEM_SHADOW);\
	}

#define ExportBasicMacro()\
	{\
		PPool->Stop();\
\
		Uint8 Memory[32768];\
		int MemBase;\
		GetBasicMemBase(MemBase);\
		C6502 *cpu = (C6502 *)PPool->GetWellDefinedComponent(COMPONENT_CPU);\
		cpu->ReadMemoryBlock(MemBase, 0, 32768, Memory);\
\
		char *NamePtr = (char *)ev.user.data1;\
		char TempString[2048];\
\
		if(! GetHost()->ExtensionIncluded(NamePtr, BASICExtensions))\
		{\
			sprintf(TempString, "%s.bas", NamePtr);\
			NamePtr = TempString;\
		}\
\
		if(!ExportBASIC( NamePtr, Memory))\
		{\
			char TempString[2048];\
			sprintf(TempString, "Unable to export BASIC code.\n%s.", GetBASICError());\
			GetHost() -> DisplayError(TempString);\
		}\
\
		PPool->Go();\
	}

#define ImportBasicMacro(name)\
	{\
		Uint8 Memory[32768];\
		int MemBase;\
		GetBasicMemBase(MemBase);\
		C6502 *cpu = (C6502 *)PPool->GetWellDefinedComponent(COMPONENT_CPU);\
		cpu->ReadMemoryBlock(MemBase, 0, 32768, Memory);\
\
		if(!ImportBASIC( name, Memory))\
		{\
			char TempString[2048];\
			sprintf(TempString, "Unable to import BASIC code.\n%s.", GetBASICError());\
			GetHost() -> DisplayError(TempString);\
		}\
		else\
			cpu->WriteMemoryBlock(MemBase, 0, 32768, Memory);\
	}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_EVERYTHING);
	bool AllowGUI;
	CProcessPool *PPool;
	
//	printf("%s\n", getenv("HOME"));
//	exit(1);

	BasicConfigurationStore * Store = GetHost()->OpenConfigurationStore("%HOMEPATH%/%DOT%electrem.cfg" );

	AllowGUI = Store ->ReadBool("AllowGUI", true);
	GetHost() -> ReadConfiguration( Store );
	GetHost() -> RegisterArgs(argc, argv);

	/* load settings */
	ElectronConfiguration Base;
	Base.Read( Store );

	/* parse all program arguments to modify configuration */
	if(argc > 1)
	{
		int iptr = 1;
		while(iptr < argc)
		{
			/* an argument or a filename? */
			if(argv[iptr][0] == '-')
			{
				if(!strcmp(argv[iptr], "-fasttape"))
					Base.FastTape = true;
				if(!strcmp(argv[iptr], "-slowtape"))
					Base.FastTape = false;
				if(!strcmp(argv[iptr], "-autoload"))
					Base.Autoload = true;
				if(!strcmp(argv[iptr], "-autoconfigure"))
					Base.Autoconfigure = true;
			}
			iptr++;
		}
	}

	PPool = new CProcessPool( Base );
	SetupBASICTables();

	GetHost() ->SetupUI( PPool, Store );

#if !defined(USE_NATIVE_GUI) && !defined(NO_GUI)
	CGUI *GUI;
#endif	

	PPool->SetDebugFlags(PPDEBUG_SCREENFAILED | PPDEBUG_GUI | PPDEBUG_FSTOGGLE | PPDEBUG_OSFAILED | PPDEBUG_BASICFAILED | PPDEBUG_KILLINSTR | PPDEBUG_UNKNOWNOP);
	PPool->IOCtl(IOCTL_SUPERRESET, NULL, 0);
	/* parse all non-arguments (consider loading stuff) */
	if(argc > 1)
	{
		int iptr = 1;
		while(iptr < argc)
		{
			/* an argument or a filename? */
			if(argv[iptr][0] != '-')
			{
				SDL_Event evt;

				evt.type = SDL_USEREVENT;
				evt.user.code = GUIEVT_LOADFILE;
				evt.user.data1 = strdup(argv[iptr]);
				SDL_PushEvent(&evt);
			}

			iptr++;
		}
	}

	char * defFile = Store->ReadString( "Load", NULL );
	if ( NULL != defFile )
	{
		PPool -> Open(defFile );
		free( defFile );
	}

	/* check for persistent state, attempt to load state if so... */
	if(Base.PersistentState)
		PPool->Open("%HOMEPATH%/%DOT%electremstate.uef");

	/* run emulation */
	PPool->Go(false);

#if !defined(USE_NATIVE_GUI) && !defined(NO_GUI)
	/* create GUI */
	GUI = new CGUI(PPool, Store);
#endif

	// handle important and/or debugging events
	bool Quit = false;
	char *BASICName = NULL;
	char *BASICExtensions[] = { "txt", "bas", "bbc", NULL };
	while(!Quit)
	{
		SDL_Event ev;
		SDL_WaitEvent(&ev);

		switch(ev.type)
		{
			case SDL_JOYBUTTONDOWN:
				printf( "Joybutton down!" );
				break;

			// Quit message from the framework
			case SDL_QUIT: 
				Quit = true; 
				break;

			// this is the conduit through which information is passed back here from the
			// emulation thread and through which the GUI invokes actions in the emulator
			case SDL_USEREVENT:
				switch(ev.user.code)
				{
					// Load a File
					case GUIEVT_LOADFILE:
					{
						/* if it's a .bas or .txt, deal with by resetting, loading as basic, and issueing a RUN */
						if(GetHost() -> ExtensionIncluded((char *)ev.user.data1, BASICExtensions))
						{
							/* complicated autoload mechanism */
							ElectronConfiguration cfg;
							PPool->GetConfiguration(cfg);
							if(cfg.Autoload)
							{
								PPool->IOCtl(IOCTL_SUPERRESET);
								PPool->SetCycleLimit(ELECTRON_RESET_TIME);
								PPool->AddDebugFlags(PPDEBUG_CYCLESDONE);
								BASICName = (char *)ev.user.data1;
							}
							else
							{
								PPool->Stop();
								ImportBasicMacro((char *)ev.user.data1);
								PPool->Go();
								free(ev.user.data1); ev.user.data1 = NULL;
							}
						}
						else
						{
							PPool->Open( (char *)ev.user.data1 );
							free(ev.user.data1); ev.user.data1 = NULL;
						}
					}
					break;

					// eject a Disc
					case GUIEVT_EJECTDISC0:
					case GUIEVT_EJECTDISC1:
					{
						PPool->Stop();
						CWD1770 *Plus3 = (CWD1770 *)PPool->GetWellDefinedComponent(COMPONENT_PLUS3);
						Plus3->Close(ev.user.code - GUIEVT_EJECTDISC0);
						PPool->Go();
					}
					break;

					// insert a Disc - never autoloads!
					case GUIEVT_INSERTDISC0:
					case GUIEVT_INSERTDISC1:
					{
						PPool->Stop();
						CWD1770 *Plus3 = (CWD1770 *)PPool->GetWellDefinedComponent(COMPONENT_PLUS3);
						if(Plus3->Open((char *)ev.user.data1, ev.user.code - GUIEVT_INSERTDISC0) == WDOPEN_FAIL)
							GetHost()->DisplayError("Unable to open disc image.");
						free(ev.user.data1); ev.user.data1 = NULL;
						PPool->Go();
					}
					break;

					// insert Tape 
					case GUIEVT_INSERTTAPE:
					{
						PPool->Stop();
						CTape *Tape = (CTape *)PPool->GetWellDefinedComponent(COMPONENT_TAPE);
						if(!Tape->Open((char *)ev.user.data1))
							GetHost()->DisplayError("Unable to open tape image.");
						free(ev.user.data1); ev.user.data1 = NULL;
						PPool->Go();
					}
					break;

					// eject Tape
					case GUIEVT_EJECTTAPE:
					{
						PPool->Stop();
						CTape *Tape = (CTape *)PPool->GetWellDefinedComponent(COMPONENT_TAPE);
						Tape->Close();
						PPool->Go();
					}
					break;

					// rewind Tape
					case GUIEVT_REWINDTAPE:
					{
						PPool->Stop();
						CTape *Tape = (CTape *)PPool->GetWellDefinedComponent(COMPONENT_TAPE);
						Tape->IOCtl(TAPEIOCTL_REWIND);
						PPool->Go();
					}
					break;

					// at the minute the only thing this is used for is squirting BASIC
					case PPDEBUG_CYCLESDONE:
					{
						ImportBasicMacro(BASICName);

						CULA *ula = (CULA *)PPool->GetWellDefinedComponent(COMPONENT_ULA);
						ula->BeginKeySequence();
							ula->TypeASCII("RUN\n");
						ula->EndKeySequence();

						PPool->RemoveDebugFlags(PPDEBUG_CYCLESDONE);
						PPool->Go();
						
						free(BASICName); BASICName = NULL;
					}
					break;

					// Save state
					case GUIEVT_SAVESTATE:
						PPool->SaveState( (char *)ev.user.data1 );
						free(ev.user.data1); ev.user.data1 = NULL;
					break;
					
					case GUIEVT_DUMPMEMORY:
					{
						PPool->MemoryDump( (char *)ev.user.data1 );
						free(ev.user.data1); ev.user.data1 = NULL;
					}
					break;

					case GUIEVT_EXPORTBASIC:
						ExportBasicMacro();
						free(ev.user.data1); ev.user.data1 = NULL;
					break;

					case GUIEVT_IMPORTBASIC:
					{
						PPool->Stop();
						ImportBasicMacro((char *)ev.user.data1);
						PPool->Go();
						free(ev.user.data1); ev.user.data1 = NULL;
					}
					break;

					// Issue a break command
					case GUIEVT_BREAK:
						PPool->IOCtl(PPOOLIOCTL_BREAK);
					break;

					// type some text
					case GUIEVT_INSERTTEXT:
					{
						CULA *ula = (CULA *)PPool->GetWellDefinedComponent(COMPONENT_ULA);
						ula->BeginKeySequence();
							ula->TypeASCII((char *)ev.user.data1);
						ula->EndKeySequence();
						free(ev.user.data1); ev.user.data1 = NULL;
					}
					break;

					// Reset the emulation
					case GUIEVT_RESET:
						PPool->IOCtl(IOCTL_SUPERRESET);
					break;

					// Pause the emulation
					case GUIEVT_PAUSE:
						PPool->Stop();
					break;

					// set printer target file
					case GUIEVT_SETPRINTERFILE:
					{
						CPlus1 *plus1 = (CPlus1 *)PPool->GetWellDefinedComponent(COMPONENT_PLUS1);
						plus1->SetPrinterTarget((char *)ev.user.data1);
						free(ev.user.data1); ev.user.data1 = NULL;

						/* check if the plus 1 is enabled */
						ElectronConfiguration CurrentConfig;
						PPool->IOCtl(IOCTL_GETCONFIG, &CurrentConfig);
						if(!CurrentConfig.Plus1)
						{
							GetHost() -> DisplayWarning("To use printer output the Plus 1 add-on must be enabled.\nThe emulator has done this for you, but you must reset the Electron for this change to take effect.");
							PPool->IOCtl(IOCTL_GETCONFIG_FINAL, &CurrentConfig);
							CurrentConfig.Plus1 = true;
							PPool->IOCtl(IOCTL_SETCONFIG, &CurrentConfig);
						}					
					}
					break;
					
					// close printer file
					case GUIEVT_CLOSEPRINTERFILE:
					{
						CPlus1 *plus1 = (CPlus1 *)PPool->GetWellDefinedComponent(COMPONENT_PLUS1);
						plus1->CloseFile();
					}
					break;
					
					// force form feed
					case GUIEVT_FORMFEED:
					{
						CPlus1 *plus1 = (CPlus1 *)PPool->GetWellDefinedComponent(COMPONENT_PLUS1);
						plus1->DoFormFeed();
					}
					break;

					case GUIEVT_CONTINUE:
						PPool->Go();
					break;

					case GUIEVT_COPY:
						GetHost()->SaveScreenShot( PPool->Display()->GetBufferCopy() );
					break;

					// Assign a new configuration to the emulation
					case GUIEVT_ASSIGNCONFIG:
						PPool->IOCtl(IOCTL_SETCONFIG, ev.user.data1);
						delete (ElectronConfiguration *)ev.user.data1; ev.user.data1 = NULL;
					break;

					// Quit the emulator
					case PPDEBUG_USERQUIT: 
						Quit = true; 
					break;

					// Error event - no screen
					case PPDEBUG_SCREENFAILED: 
						GetHost() -> DisplayError("Unable to create display");
						Quit = true; 
					break;

					// Error event - no OS
					case PPDEBUG_OSFAILED: 
						GetHost() -> DisplayError("Unable to load the OS ROM");
						Quit = true; 
					break;

					// Error event - no BASIC
					case PPDEBUG_BASICFAILED: 
						GetHost() -> DisplayError("Unable to load the BASIC ROM");
						Quit = true; 
					break;

					// Error event - KILL instruction
					case PPDEBUG_KILLINSTR:
						GetHost() -> DisplayError("The Electron has been put into a dead state.\nThe emulator will now reset.");
						PPool->IOCtl(IOCTL_SUPERRESET);
					break;

					case PPDEBUG_UNKNOWNOP:
						GetHost() -> DisplayError("The CPU has encountered an unknown opcode.\nThe emulator will now reset.");
						PPool->IOCtl(IOCTL_SUPERRESET);
					break;

#if !defined( USE_NATIVE_GUI ) && !defined(NO_GUI)
					// Keypress actions
					case PPDEBUG_GUI:
						if(AllowGUI)
						{
							PPool->Stop();
							Quit |= GUI->Go();
							if(!Quit) PPool->Go(false);
						}
					break;
#endif

					// this PPDEBUG can also be generated by the GUI, so...
					case PPDEBUG_FSTOGGLE:
						PPool->Stop(); // Must stop emulator
#if defined( USE_NATIVE_GUI )
						GetHost() ->FullScreen(((CDisplay *)PPool->GetWellDefinedComponent(COMPONENT_DISPLAY))->QueryFullScreen() == false);
#endif
						((CDisplay *)PPool->GetWellDefinedComponent(COMPONENT_DISPLAY))->ToggleFullScreen();
						PPool->Go();
					break;
				}
			break;
		}
		GetHost() -> ProcessEvent( &ev, PPool );
	}

	if(BASICName)
		free(BASICName);

	/* check for persistent state, attempt to save state if so... */
	if(Base.PersistentState)
		PPool->SaveState( "%HOMEPATH%/%DOT%electremstate.uef" );
	else
	{
		char *fname = GetHost()->ResolveFileName("%HOMEPATH%/%DOT%electremstate.uef");
		unlink(fname);
		delete[] fname;
	}

	// stop emulation thread
	PPool->Stop();

	// store latest machine state
	PPool->IOCtl(IOCTL_GETCONFIG_FINAL, &Base);
	Base.Write(Store);
	Store -> Flush();

#if !defined(USE_NATIVE_GUI) && !defined(NO_GUI)
	delete GUI;
#endif
	delete PPool;
	delete Store;
	SDL_Quit();
	return 0;
}
