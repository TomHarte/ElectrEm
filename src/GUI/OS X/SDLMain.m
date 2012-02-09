/*
	[SDL credits]

	SDLMain.m - main entry point for our Cocoa-ized SDL app
		Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
		Non-NIB-Code & other changes: Max Horn <max@quendi.de>

	Feel free to customize this file to suit your needs
	
	[ElectrEm credits]

	Customised by Ewen Roberts for ElectrEm, cruft removed, event handlers added, and launching of SDL_Main tidied up so it can
	handle open file requests before the SDLMain is called.

	Further customised by Thomas Harte, as new features are added to the emulator, etc.
*/

#include "SDL.h"
#include "SDLMain.h"
#include <sys/param.h> /* for MAXPATHLEN */
#include <unistd.h>
#include "../../GUIEvents.h"
#include "Preferences.h"
#include "SDL_thread.h"

#define preferences [NSUserDefaults standardUserDefaults]

static BOOL isFullyLaunched = NO;
BOOL allowFullKeyboardInput = NO;

@interface SDLApplication : NSApplication
@end

@implementation SDLApplication
/* Invoked from the Quit menu item */
- (void)terminate:(id)sender
{
	/* Post an SDL_QUIT event */
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

- (void)sendEvent:(NSEvent *)anEvent
{
	/* filter out keys with command pressed */
	if( NSKeyDown == [anEvent type] || NSKeyUp == [anEvent type] )
	{
		if( [anEvent modifierFlags] & NSCommandKeyMask || allowFullKeyboardInput) 
			[super sendEvent: anEvent];
	}
	else 
		[super sendEvent: anEvent];
}
@end

/* The main class of the application, the application's delegate */
@implementation SDLMain

int argc = 1;
char ** argV;
int status;
int MainHelper(void *data)
{
	status = SDL_main (argc, argV);
	return 0;
}

/* Called when the internal event loop has just started running */
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
	isFullyLaunched = YES;
	isPaused = NO;

	/*	Make up the argv/argc params for the SDL code.
		param 1 is the path of the application bundle
		If the user launched this app by double clicking a file, param2 is the name of the file */
	if (NULL != currentFile )
	{
		argc = 2;
	}

	argV = (char**)malloc( sizeof( char * ) * argc );
	argV[0] = strdup( [ [[NSBundle mainBundle] bundlePath] cString ] );

	if ( NULL != currentFile )
		argV[1] = strdup( [currentFile cString ] );

	/* Build default settings */
	[ self setDefaultPreferences];

	/* enable command+(blah) sequences */
	setenv ("SDL_ENABLEAPPEVENTS", "1", 1); 
//	[ nsWindowObject makeKeyAndOrderFront:nil ];

	// set things up so that we know which insert/eject disc is which
//	[insertDisc0 setTag:0];
//	[ejectDisc0 setTag:0];
//	[insertDisc1 setTag:1];
//	[ejectDisc1 setTag:1];

	// can't eject any discs yet, because none have been inserted
//	[ejectDisc0 setAction:nil];
//	[ejectDisc1 setAction:nil];


	/* mess around with ROMs menu, for a test! */
	int c = 8;
	while(c--)
		romMenuPointers[c] = NULL;
		
	[insertROM setEnabled:NO];

	/* Hand off to main application code */
	status = SDL_main (argc, argV);
//	SDL_Thread *NewThread = SDL_CreateThread(MainHelper, NULL);
//	SDL_WaitThread(NewThread, NULL);

	/* Kinda pointless, but it makes me feel good to tidy up */
	int i;
	for (i = 0; i < argc; i ++ )
	{
		free( argV[i]);
	}
	free( argV );

	/* We're done, thank you for playing */
	exit(status);
}


-(void) setDefaultPreferences
{
	NSMutableDictionary *defaultPrefs = [NSMutableDictionary dictionary];

	// put default prefs in the dictionary
	[defaultPrefs setObject: [NSNumber numberWithBool:NO] forKey: @"Plus1"];
	[defaultPrefs setObject: [NSNumber numberWithBool:NO] forKey: @"FirstByte"];
	[defaultPrefs setObject: [NSNumber numberWithBool:YES] forKey: @"FastTape"];
	[defaultPrefs setObject: [NSNumber numberWithBool:YES] forKey: @"Autoload"];
	[defaultPrefs setObject: [NSNumber numberWithBool:YES] forKey: @"Autoconfigure"];
	[defaultPrefs setObject: [NSNumber numberWithBool:YES] forKey: @"Plus3"];
	[defaultPrefs setObject: [NSNumber numberWithBool:NO] forKey: @"Drive1WriteProtect"];
	[defaultPrefs setObject: [NSNumber numberWithBool:NO] forKey: @"Drive2WriteProtect"];

	[defaultPrefs setObject: [NSNumber numberWithBool:YES] forKey: @"DisplayMultiplexed"];
	[defaultPrefs setObject: [NSNumber numberWithBool:NO] forKey: @"AllowOverlay"];
	[defaultPrefs setObject: [NSNumber numberWithBool:NO] forKey: @"StartFullScreen"];

	[defaultPrefs setObject: [NSNumber numberWithInt:64] forKey: @"Volume"];
	[defaultPrefs setObject: [NSNumber numberWithInt:0] forKey: @"SloggerMRB"];

	[defaultPrefs setObject: [NSNumber numberWithBool:YES] forKey: @"AutoLineFeed"];
	[defaultPrefs setObject: [NSNumber numberWithBool:NO] forKey: @"UseSlashedZero"];
	[defaultPrefs setObject: [NSNumber numberWithBool:YES] forKey: @"PrintInColour"];
	[defaultPrefs setObject: [NSNumber numberWithInt:3] forKey: @"PrintNationality"];
	[defaultPrefs setObject: [NSNumber numberWithInt:0] forKey: @"PrintPaperSize"];
	[defaultPrefs setObject: [NSNumber numberWithInt:0] forKey: @"PrintWeight"];
	[defaultPrefs setObject: [NSNumber numberWithInt:0] forKey: @"PrintPitch"];

	// register the dictionary of defaults
	[[NSUserDefaults standardUserDefaults] registerDefaults: defaultPrefs];
}

// called when user opens a file through the Finder
- (BOOL)application:(NSApplication *)theApplication
	openFile:(NSString *)filename
{
	if ( YES == isFullyLaunched )
	{
		// Send an event to the SDL code telling it to load the specified file.
		SDL_Event evt;
		evt.type = SDL_USEREVENT;
		evt.user.code = GUIEVT_LOADFILE;
		evt.user.data1 = strdup( [ filename cString ] );
		SDL_PushEvent( &evt );

		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:filename]];
	}
	else // Can't launch until emulator is ready, keep hold of the filename though
	{
		currentFile = [NSString stringWithString:filename ];
	}

	return YES;
}

// called when the user issues a pasteboard paste command
- (IBAction)editPaste:(id)sender
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSString *type = [pasteboard availableTypeFromArray:[NSArray arrayWithObject:NSStringPboardType]];
	if(type != nil) 
	{
		NSString *contents = [pasteboard stringForType:type];
		if (contents != nil)
		{
			const char *str = [contents cString];
			if(str)
			{
				SDL_Event evt;
				evt.type = SDL_USEREVENT;
				evt.user.code = GUIEVT_INSERTTEXT;
				evt.user.data1 = (void *) strdup( str );
				SDL_PushEvent( &evt );
			}
//			[contents release];
		}
	}
}

- (IBAction)editCopy:(id)sender
{
	SDL_Event evt;
	evt.type = SDL_USEREVENT;
	evt.user.code = GUIEVT_COPY;
	SDL_PushEvent( &evt );
}

- (IBAction)fullScreenEmulator:(id)sender
{
	SDL_Event evt;
	evt.type = SDL_USEREVENT;
	evt.user.code = GUIEVT_FULLSCREEN;
	SDL_PushEvent( &evt );		
}

// File -> Open action, open a Panel for the user to find their file and then pass on an event to the emulator
// telling it to open that file.
- (IBAction)openGame:(id)sender
{
	allowFullKeyboardInput = YES;

	NSString *path = nil;
	NSOpenPanel *openPanel = [ NSOpenPanel openPanel ];
	NSString * dirPath = [ preferences objectForKey:@"LastOpenPath"];

	if ( [ openPanel runModalForDirectory:dirPath file:@"Saved Game"
							types:[NSArray arrayWithObjects:	@"uef", @"uef.gz",
																@"adf", @"adf.gz", @"adl", @"adl.gz", @"adm", @"adm.gz",
																@"ssd", @"ssd.gz", @"dsd", @"dsd.gz", @"img", @"img.gz",
																@"fdi",
																@"bas", @"bbc", @"txt", 
																nil]] ) 
	{
		path = [ [ openPanel filenames ] objectAtIndex:0 ];
		[preferences setObject:[openPanel directory] forKey:@"LastOpenPath" ];
	
		currentFile = [NSString stringWithString:path ];

		SDL_Event evt;
		evt.type = SDL_USEREVENT;
		evt.user.code = GUIEVT_LOADFILE;
		evt.user.data1 = strdup( [ path cString ] );
		SDL_PushEvent( &evt );

		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:currentFile]];
		[preferences synchronize ]; 
	}

	allowFullKeyboardInput = NO;
}

// insert Disc
- (IBAction)insertDisc:(id)sender
{
	allowFullKeyboardInput = YES;

	NSString *path = nil;
	NSOpenPanel *openPanel = [ NSOpenPanel openPanel ];
	NSString * dirPath = [ preferences objectForKey:@"LastOpenPath"];

	if ( [ openPanel runModalForDirectory:dirPath file:@"Saved Game"
							types:[NSArray arrayWithObjects:	@"uef", @"uef.gz",
																@"adf", @"adf.gz", @"adl", @"adl.gz", @"adm", @"adm.gz",
																@"ssd", @"ssd.gz", @"dsd", @"dsd.gz", @"img", @"img.gz",
																@"fdi",
																nil]] ) 
	{
		path = [ [ openPanel filenames ] objectAtIndex:0 ];
		[preferences setObject:[openPanel directory] forKey:@"LastOpenPath" ];

		currentFile = [NSString stringWithString:path ];

		SDL_Event evt;
		evt.type = SDL_USEREVENT;
		evt.user.code = GUIEVT_INSERTDISC0 + [sender tag];
		evt.user.data1 = strdup( [ path cString ] );
		SDL_PushEvent( &evt );

		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:currentFile]];
		[preferences synchronize ]; 
	}

	allowFullKeyboardInput = NO;
}

// eject Disc
- (IBAction)ejectDisc:(id)sender
{
	SDL_Event evt;
	evt.user.type = SDL_USEREVENT;
	evt.user.code = GUIEVT_EJECTDISC0 + [sender tag];
	SDL_PushEvent( &evt );
}

// insert tape
- (IBAction)insertTape:(id)sender
{
	allowFullKeyboardInput = YES;

	NSString *path = nil;
	NSOpenPanel *openPanel = [ NSOpenPanel openPanel ];
	NSString * dirPath = [ preferences objectForKey:@"LastOpenPath"];

	if ( [ openPanel runModalForDirectory:dirPath file:@"Saved Game"
							types:[NSArray arrayWithObjects:	@"uef", @"uef.gz",
																nil]] ) 
	{
		path = [ [ openPanel filenames ] objectAtIndex:0 ];
		[preferences setObject:[openPanel directory] forKey:@"LastOpenPath" ];

		currentFile = [NSString stringWithString:path ];

		SDL_Event evt;
		evt.type = SDL_USEREVENT;
		evt.user.code = GUIEVT_INSERTTAPE;
		evt.user.data1 = strdup( [ path cString ] );
		SDL_PushEvent( &evt );

		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:currentFile]];
		[preferences synchronize ]; 
	}

	allowFullKeyboardInput = NO;
}

// eject Tape
- (IBAction)ejectTape:(id)sender
{
	SDL_Event evt;
	evt.user.type = SDL_USEREVENT;
	evt.user.code = GUIEVT_EJECTTAPE;
	SDL_PushEvent( &evt );
}

// rewind Tape
- (IBAction)rewindTape:(id)sender
{
	SDL_Event evt;
	evt.user.type = SDL_USEREVENT;
	evt.user.code = GUIEVT_REWINDTAPE;
	SDL_PushEvent( &evt );
}

// ROMs -> Add ROM action Ñ open a panel so that the user can find the ROM they want to insert, add it to the ROM list
- (IBAction)addROM:(id)sender
{
	allowFullKeyboardInput = YES;

	/* find first free spot */
	int c = 0;
	while(romMenuPointers[c])
	{
		c++;
		if(c == 8)
		{
			/* this should never happen! */
			return;
		}
	}
	romMenuPointers[c] = [ROMmenu addItemWithTitle: @"Remove <ROM name>" action: @selector(removeROM:) keyEquivalent:@""];
	[romMenuPointers[c] setTag:c];	//set tag, so we know how to remove this one later

	allowFullKeyboardInput = NO;
}

// remove ROM action Ñ triggered by synthetic menu entries, used to remove a ROM (at next reboot, anyway)
- (IBAction)removeROM:(id)sender
{
	// just stupidly remove tag for now - also need to send a message to the emulator saying "stop using ROM ..."
	[ROMmenu removeItem:romMenuPointers[[sender tag]]];
	romMenuPointers[[sender tag]] = NULL;
}

// import BASIC
- (IBAction)importBASIC:(id)sender
{
	allowFullKeyboardInput = YES;

	NSString *path = nil;
	NSOpenPanel *openPanel = [ NSOpenPanel openPanel ];
	NSString * dirPath = [ preferences objectForKey:@"LastBASICPath"];

	if ( [ openPanel runModalForDirectory:dirPath file:@"Saved Game" types:[NSArray arrayWithObjects: @"bas", @"bbc", @"txt", nil]] ) 
	{
		path = [ [ openPanel filenames ] objectAtIndex:0 ];
		[preferences setObject:[openPanel directory] forKey:@"LastBASICPath" ];
	
		currentFile = [NSString stringWithString:path ];

		SDL_Event evt;
		evt.type = SDL_USEREVENT;
		evt.user.code = GUIEVT_IMPORTBASIC;
		evt.user.data1 = strdup( [ path cString ] );
		SDL_PushEvent( &evt );
	}

	allowFullKeyboardInput = NO;
}

// export BASIC
- (IBAction)extractBASIC:(id)sender
{
	allowFullKeyboardInput = YES;

	NSString *path = nil;
	NSSavePanel *savePanel = [ NSSavePanel savePanel ];
	NSString * dirPath = [ preferences objectForKey:@"LastBASICPath"];

	[savePanel setAllowedFileTypes:[NSArray arrayWithObjects:@"bas",@"bbc",@"txt",nil]];
	if ( [ savePanel runModalForDirectory:dirPath file:@"BASIC Source"] ) 
	{
		path = [ savePanel filename ];
		[preferences setObject:[savePanel directory] forKey:@"LastBASICPath" ];

		currentFile = [NSString stringWithString:path ];

		SDL_Event evt;
		evt.type = SDL_USEREVENT;
		evt.user.code = GUIEVT_EXPORTBASIC;
		evt.user.data1 = strdup( [ path cString ] );
		SDL_PushEvent( &evt );
	}

	allowFullKeyboardInput = NO;
}

// File -> Save State action, open a Panel for the user to name their file and then pass on an event to the emulator
// telling it to open that file.
- (IBAction)saveState:(id)sender
{
	allowFullKeyboardInput = YES;

	NSString *path = nil;
	NSSavePanel *savePanel = [ NSSavePanel savePanel ];
	NSString * dirPath = [ preferences objectForKey:@"LastOpenPath"];

	[savePanel setRequiredFileType:@"uef"];
	if ( [ savePanel runModalForDirectory:dirPath file:@"Saved Game" ] ) 
	{
		path = [ savePanel filename ];
		[preferences setObject:[savePanel directory] forKey:@"LastOpenPath" ];

		currentFile = [NSString stringWithString:path ];

		SDL_Event evt;
		evt.type = SDL_USEREVENT;
		evt.user.code = GUIEVT_SAVESTATE;
		evt.user.data1 = strdup( [ path cString ] );
		SDL_PushEvent( &evt );

		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:currentFile]];
		[preferences synchronize ]; 
	}

	allowFullKeyboardInput = NO;
}

- (IBAction)dumpMemory:(id)sender
{
	allowFullKeyboardInput = YES;

	NSString *path = nil;
	NSSavePanel *savePanel = [ NSSavePanel savePanel ];
	NSString * dirPath = [ preferences objectForKey:@"LastOpenPath"];

	[savePanel setRequiredFileType:@"dmp"];
	if ( [ savePanel runModalForDirectory:dirPath file:@"Memory Dump" ] ) 
	{
		path = [ savePanel filename ];
		[preferences setObject:[savePanel directory] forKey:@"LastOpenPath" ];

		currentFile = [NSString stringWithString:path ];

		SDL_Event evt;
		evt.type = SDL_USEREVENT;
		evt.user.code = GUIEVT_DUMPMEMORY;
		evt.user.data1 = strdup( [ path cString ] );
		SDL_PushEvent( &evt );

		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:currentFile]];
		[preferences synchronize ]; 
	}

	allowFullKeyboardInput = NO;
}

// File -> Set Printer State action, open a Panel for the user to name their file and then pass on an event to the emulator
// telling it to use that file. Type of printer output is decided by file type
- (IBAction)setPrinterTarget:(id)sender
{
	allowFullKeyboardInput = YES;

	NSString *path = nil;
	NSSavePanel *savePanel = [ NSSavePanel savePanel ];
	NSString * dirPath = [ preferences objectForKey:@"LastPrinterPath"];

	[savePanel setAllowedFileTypes:[NSArray arrayWithObjects:@"pdf",@"png",@"rtf",@"txt",nil]];
	[savePanel setCanSelectHiddenExtension:YES];
	[savePanel setExtensionHidden:NO];
	if( [ savePanel runModalForDirectory:dirPath file:@"Electron Printer Output"] ) 
	{
		path = [ savePanel filename ];
		[preferences setObject:[savePanel directory] forKey:@"LastPrinterPath" ];

		currentFile = [NSString stringWithString:path ];

		SDL_Event evt;
		evt.type = SDL_USEREVENT;
		evt.user.code = GUIEVT_SETPRINTERFILE;
		evt.user.data1 = strdup( [ path cString ] );
		SDL_PushEvent( &evt );
	}

	allowFullKeyboardInput = NO;
}

- (IBAction)closePrinterTarget:(id)sender
{
	SDL_Event evt;
	evt.type = SDL_USEREVENT;
	evt.user.code = GUIEVT_CLOSEPRINTERFILE;
	SDL_PushEvent( &evt );
}

- (IBAction)formFeed:(id)sender
{
	SDL_Event evt;
	evt.type = SDL_USEREVENT;
	evt.user.code = GUIEVT_FORMFEED;
	SDL_PushEvent( &evt );
}

// Internal function used to launch a file from the app bundle's resources folder.
-(void) openResourceFile:( NSString *) fileName
{
	NSString *myBundle = [[NSBundle mainBundle] bundlePath];
	NSString *fullpath = [NSString stringWithFormat:@"%@/Contents/Resources/%@",myBundle, fileName];
	[[NSWorkspace sharedWorkspace] openFile:fullpath ];
}


// Emulator -> Reset action. Sends a message to the emulator telling it to reset the Electron.
- (IBAction)resetEmulator:(id)sender
{
	SDL_Event evt;
	evt.type = SDL_USEREVENT;
	evt.user.code = GUIEVT_RESET;
	SDL_PushEvent( &evt );
}

// Emulator -> Break action. Sends a message to the emulator telling it to issue a soft-break.
- (IBAction)breakEmulator:(id)sender
{
	SDL_Event evt;
	evt.type = SDL_USEREVENT;
	evt.user.code = GUIEVT_BREAK;
	SDL_PushEvent( &evt );
}

// Help -> ElectrEm Help action.
- (IBAction)showHelp:(id)sender
{
	[self openResourceFile:@"ElectrEm Help.rtf"];
}

// Help -> Licence action.
- (IBAction)showLicense:(id)sender
{
	[self openResourceFile:@"Licence"];
}

// Help -> Readme action.
- (IBAction)showElectronWorld:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://www.acornelectron.co.uk/"]];
}

// Help -> Change Log action.
- (IBAction)showChangeLog:(id)sender
{
	[self openResourceFile:@"Changelog.html"];
}

// Help -> ElectrEm WebSite action.
- (IBAction)showStairway:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://www.stairwaytohell.com/"]];
} 

// Help -> ElectrEm WebSite action.
- (IBAction)showWebSite:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://electrem.acornelectron.co.uk/"]];
} 

// show the preferences pane
- (IBAction)preferencesEmulator:(id)sender
{
	if ( nil != prefs )
	{
		// if the preferences pane was open, release it.
		[prefs release];
	}
	prefs = [Preferences alloc];
	[NSBundle loadNibNamed:@"Preferences" owner:prefs];
}

// Show the keyboard map
- (IBAction)toggleKeyboardMap:(id)sender
{
	int mode = [sender selectedColumn];
	[keyboardMac setHidden:(1!=mode)];
	[keyboardElk setHidden:(0!=mode)];
	[keyboardPC setHidden:(2!=mode)];
}

- (NSString *)windowTitleForDocumentDisplayName:(NSString *)displayName
{
	if ( NULL == currentFile )
	{
		return displayName;
	}
	return [displayName stringByAppendingFormat:@"%@", currentFile];
}

@end


#ifdef main
# undef main
#endif

/* Main entry point to executable - should *not* be SDL_main! */
int main (int argc, const char *argv[])
{
	[SDLApplication poseAsClass:[NSApplication class]];
	NSApplicationMain (argc, argv);

	return 0;
}
