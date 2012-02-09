/* Preferences */

#include <Cocoa/Cocoa.h>

@interface Preferences : NSObject
{
    IBOutlet NSButton *autoconfigure;
    IBOutlet NSButton *autoload;
    IBOutlet NSButton *fastTapeToggle;
    IBOutlet NSButton *firstByteToggle;
    IBOutlet NSButton *multiplexedToggle;
    IBOutlet NSButton *plus1Toggle;
    IBOutlet NSButton *plus3Toggle;
    IBOutlet NSWindow *prefWindow;
    IBOutlet NSMatrix *sloggerMode;
    IBOutlet NSButton *sloggerToggle;
    IBOutlet NSSlider *volume;
    IBOutlet NSButton *drive1wptoggle;
    IBOutlet NSButton *drive2wptoggle;
    IBOutlet NSButton *startFullscreenToggle;
    IBOutlet NSButton *saveStateToggle;

    IBOutlet NSButton *printAutoLineFeed;
    IBOutlet NSButton *printUseSlashedZero;
    IBOutlet NSMatrix *printColour;
	IBOutlet NSPopUpButton *printNationality;
	IBOutlet NSPopUpButton *printPaperSize;
	IBOutlet NSPopUpButton *printPitch;
	IBOutlet NSPopUpButton *printWeight;
}
- (IBAction)togglePrintOption:(id)sender;
- (IBAction)toggleOption:(id)sender;
- (void)updateEnable;
- (void)awakeFromNib;
@end
