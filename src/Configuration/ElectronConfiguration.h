/*
 *  ElectronConfiguration.h
 *  ElectrEm
 *
 *  Created by Ewen Roberts on Sat Jul 17 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */

// A class that contains configuration information for the emulator

#ifndef __ELECTRONCONFIGURATION_H
#define __ELECTRONCONFIGURATION_H

#include "ConfigurationStore.h"

enum MRBModes
{
	MRB_OFF, MRB_TURBO, MRB_SHADOW, MRB_4Mhz, MRB_UNDEFINED
};

#define MAXNUM_EXTRAROMS		7

class ElectronConfiguration
{
public:

	bool Plus1,
		 FirstByte;
	bool FastTape,
		 Autoload,
		 Autoconfigure;
	MRBModes MRBMode;

	bool Jim;
	bool PersistentState;

	struct
	{
		bool Drive1WriteProtect;
		bool Drive2WriteProtect;
		bool Enabled;
	} Plus3;

	struct
	{
		bool AllowOverlay;
		bool DisplayMultiplexed;
		bool StartFullScreen;
		bool UseOpenGL;
	} Display;

	char * ROMPath,
		 * GFXPath;

	struct
	{
		char * ROMFileName;
	} ExtraROMs[MAXNUM_EXTRAROMS];

	int Volume;

	struct
	{
		int Nationality;
		int PaperSize;
		int PrintPitch;
		int PrintWeight;
		bool AutoLineFeed;
		bool UseSlashedZero;
		bool Colour;
	} Printer;

public:
	ElectronConfiguration();	
	ElectronConfiguration(const ElectronConfiguration &);	//copy constructor
	virtual ~ElectronConfiguration();
	void operator =(const ElectronConfiguration &); //overloaded = parameter

	bool operator !=(ElectronConfiguration &rvalue);
	void Write( BasicConfigurationStore * store );	
	void Read( BasicConfigurationStore * store );
};

#endif
