/*   SDLMain.m - main entry point for our Cocoa-ized SDL app
       Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
       Non-NIB-Code & other changes: Max Horn <max@quendi.de>

    Feel free to customize this file to suit your needs
	
	Customised by Ewen Roberts for ElectrEm.
*/

#import <Cocoa/Cocoa.h>
#import "Preferences.h"
@interface SDLMain : NSObject
{
	BOOL isPaused;
	NSString * currentFile;
	NSString * fileToOpen;
	Preferences * prefs;
	IBOutlet NSImageView *keyboardElk;
	IBOutlet NSImageView *keyboardMac;
	IBOutlet NSImageView *keyboardPC;
	IBOutlet NSMenu *ROMmenu;

	IBOutlet NSMenuItem *insertROM;

	NSMenuItem *romMenuPointers[8];
}

- (IBAction)editCopy:(id)sender;
- (IBAction)editPaste:(id)sender;
- (IBAction)openGame:(id)sender;
- (IBAction)saveState:(id)sender;
- (IBAction)addROM:(id)sender;
- (IBAction)removeROM:(id)sender;
- (IBAction)extractBASIC:(id)sender;
- (IBAction)importBASIC:(id)sender;
- (IBAction)setPrinterTarget:(id)sender;
- (IBAction)closePrinterTarget:(id)sender;
- (IBAction)formFeed:(id)sender;
- (IBAction)fullScreenEmulator:(id)sender;
- (IBAction)insertDisc:(id)sender;
- (IBAction)ejectDisc:(id)sender;
- (IBAction)insertTape:(id)sender;
- (IBAction)ejectTape:(id)sender;
- (IBAction)rewindTape:(id)sender;

- (IBAction)resetEmulator:(id)sender;
- (IBAction)breakEmulator:(id)sender;
- (IBAction)showStairway:(id)sender;
- (IBAction)showWebSite:(id)sender;
- (IBAction)showElectronWorld:(id)sender;
- (IBAction)showChangeLog:(id)sender;
- (IBAction)showHelp:(id)sender;
- (IBAction)showLicense:(id)sender;
- (IBAction)preferencesEmulator:(id)sender;
- (IBAction)toggleKeyboardMap:(id)sender;

- (BOOL)application:(NSApplication *)theApplication
           openFile:(NSString *)filename;
		   
- (void)openResourceFile:( NSString *) fileName;
- (void)setDefaultPreferences;
- (NSString *)windowTitleForDocumentDisplayName:(NSString *)displayName;

@end
