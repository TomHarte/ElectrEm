/*
 *  ROMInfoMain.cpp
 *  ElectrEm
 *
 *  Created by Ewen Roberts on 20/02/2005.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */

#include "sdl.h"
#include "ROMChecker.h"

#include <iostream>

using namespace std;

int main (int argc, const char *argv[])
{
	if ( argc < 2 )
	{
		cout << "ROMInfo: prints out information on Acorn format ROM images." 
			<< endl 
			<< "Usage: ROMInfo <ROM path 1> [<ROM path 2> ... <ROM path n>]"
			<< endl;
		return 1;
	}
	
	ROMImage rom;

	for ( int i = 1; i < argc; i ++ )
	{
		if ( rom.LoadImage( argv[i] ) )
		{
			cout << argv[i] << endl;
			cout << "Title " << rom.Title() << endl;
			cout << "Copyright " << rom.Copyright() << endl;
			cout << "Version " << rom.Version() << endl;
			cout << "Type " << ( rom.IsLanguageRom() ? "Language" : "Service" ) << endl;
			cout << endl;
		}
		else
		{
			cout << argv[ i ] << " is an invalid ROM image." << endl;
		}
		
	}
	return 0;
}
