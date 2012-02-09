//
//  CocoaHelpers.m
//  ElectrEm
//
//  Created by Ewen Roberts on Sun Jul 18 2004.
//	Modified at various other times by Thomas Harte.
//
#import <Cocoa/Cocoa.h>
#include "../Configuration/MacConfigurationStore.h"
#include "HostMachine.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "MacHostMachine.h"

static MacHostMachine macHost;

HostMachine * GetHost()
{
	return & macHost;
}

MacHostMachine::MacHostMachine()
{
	autoreleasepool = [[NSAutoreleasePool alloc] init];
	resourcesPath = [NSString stringWithFormat:@"%@/Contents/Resources", [[NSBundle mainBundle] bundlePath] ];
	NSString * romPath = [NSString stringWithFormat:@"%@/ROMs", resourcesPath ];
	RegisterPath( "%ROMPATH%", [romPath cString ] );
}

MacHostMachine::~MacHostMachine()
{
//	[autoreleasepool release ];
}

void MacHostMachine::CloseConfigurationStore( BasicConfigurationStore * store )
{
}

char MacHostMachine::DirectorySeparatorChar() const
{
	return '/';
}

extern BOOL allowFullKeyboardInput;
// Displays a warning dialog bearing the message fmt, and writes fmt to the log.
void MacHostMachine::DisplayError(char * fmt, ... )
{
	va_list Arguments;

	va_start(Arguments, fmt);
	// Make an NSString with the error message formatted.
	NSString * msg = [[NSString alloc] initWithFormat:[NSString stringWithCString:fmt] arguments:Arguments];
	va_end(Arguments);
	
	NSString *title = @"Error";

	// Show the alert
	allowFullKeyboardInput = YES;
	NSRunAlertPanel(title, msg, nil, nil, nil);
	allowFullKeyboardInput = NO;
	[ msg release ];

	// Log message to stderr.
	NSLog( msg );
}

// Displays a warning dialog bearing the message fmt
void MacHostMachine::DisplayWarning(char * fmt, ... )
{
	va_list Arguments;

	va_start(Arguments, fmt);
	// Make an NSString with the error message formatted.
	NSString * msg = [[NSString alloc] initWithFormat:[NSString stringWithCString:fmt] arguments:Arguments];
	va_end(Arguments);

	// Show the alert
	NSString *title = @"Warning";

	// Show the alert
	allowFullKeyboardInput = YES;
	NSRunAlertPanel(title, msg, nil, nil, nil);
	allowFullKeyboardInput = NO;
	[ msg release ];
}

char *MacHostMachine::GetExecutablePath()
{
	return strdup( [ resourcesPath cString ] );
}

FileDesc *MacHostMachine::GetFolderContents( const char *) 
{
	return NULL;
}

FileSpecs MacHostMachine::GetSpecs( const char * )
{
	FileSpecs empty;
	empty.Size = sizeof(FileSpecs);
	empty.Stats = 0;
	return empty;
}

int MacHostMachine::MaxPathLength() const
{
	return 1024;
}

BasicConfigurationStore * MacHostMachine::OpenConfigurationStore( const char *)
{
	return new MacConfigurationStore();
}

bool MacHostMachine::ProcessEvent( SDL_Event * evt, CProcessPool * pool )
{
	return false;
}

void MacHostMachine::ReadConfiguration( BasicConfigurationStore *)
{
}

//
// Copy the buffer from an SDL surface to an NSImage and place that image on the pasteboard as a TIFF
//
void MacHostMachine::SaveScreenShot( SDL_Surface * surface )
{
	if ( NULL == surface )
	{
		DisplayError( "Unable to copy screen buffer" );
		return;
	}
	printf( "Copy Surface:\nSize - %dx%d\nOffset - %d\nPitch - %d\nRefcount - %d", surface->w, surface->h, surface->offset, (int)surface->pitch, surface->refcount );

	// Create an NSImage the same size as the SDL surface
	NSSize sz;
	sz.width = surface->w;
	sz.height = surface->h; 
	NSImage * img = [ [NSImage alloc ] initWithSize: sz ];

	// Create a bitmap image representation to put things in
	NSBitmapImageRep * rep = [NSBitmapImageRep alloc ];
	[rep initWithBitmapDataPlanes:NULL
		pixelsWide:surface->w 
		pixelsHigh:surface->h 
		bitsPerSample:8 
		samplesPerPixel:3
		hasAlpha:NO
		isPlanar:NO
		colorSpaceName:NSCalibratedRGBColorSpace
		bytesPerRow:0 
        bitsPerPixel:0 ];

	// Copy the SDL surfaces data to the bitmap representation
	memcpy( [rep bitmapData], surface->pixels, (surface->w * 3) * surface->h);
	// Add the bitmap representation to the NSImage
	[img addRepresentation: rep];

	// Add the image to the pasteboard
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard 
		declareTypes:[NSArray arrayWithObject:NSTIFFPboardType] 
		owner:nil ];
	[pasteboard 
		setData:[img TIFFRepresentation]
		forType:NSTIFFPboardType ];

	// Free the image and representation
	[img release];
	[rep release];

	// Free the surface by calling baseclass
	HostMachine::SaveScreenShot( surface );
}
