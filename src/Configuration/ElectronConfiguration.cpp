/*
 *  ElectronConfiguration.cpp
 *  ElectrEm
 *
 *  Created by Ewen Roberts on Sat Jul 17 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ElectronConfiguration.h"

ElectronConfiguration::ElectronConfiguration()
{
	/* default configeration */
	Plus1 = Plus3.Enabled = FirstByte = false;
	FastTape = true;
	MRBMode = MRB_OFF;
	ROMPath = NULL;
	GFXPath = NULL;
	Autoload = Autoconfigure = true;
	Plus3.Drive1WriteProtect = Plus3.Drive2WriteProtect = false;
	Display.AllowOverlay = Display.DisplayMultiplexed = true;
	Display.StartFullScreen = false;
	Volume = 128;
	Jim = false;
	PersistentState = false;

	int c = MAXNUM_EXTRAROMS;
	while(c--)
		ExtraROMs[c].ROMFileName = NULL;
}

ElectronConfiguration::~ElectronConfiguration()
{
	if(ROMPath) free(ROMPath);
	if(GFXPath) free(GFXPath);

	int c = MAXNUM_EXTRAROMS;
	while(c--)
		if(ExtraROMs[c].ROMFileName)
			free(ExtraROMs[c].ROMFileName);
}

ElectronConfiguration::ElectronConfiguration(const ElectronConfiguration &i)
{
	memcpy(this, &i, sizeof(ElectronConfiguration));

	/* make local copies of any strings - to avoid memory problems later */
	if(ROMPath)
		ROMPath = strdup(ROMPath);
	if(GFXPath)
		GFXPath = strdup(GFXPath);
	int c = MAXNUM_EXTRAROMS;
	while(c--)
		if(ExtraROMs[c].ROMFileName)
			ExtraROMs[c].ROMFileName = strdup(ExtraROMs[c].ROMFileName);
}

void ElectronConfiguration::operator =(const ElectronConfiguration &i)
{
	/* free strings, if currently allocated */
	if(ROMPath) free(ROMPath);
	if(GFXPath) free(GFXPath);

	int c = MAXNUM_EXTRAROMS;
	while(c--)
		if(ExtraROMs[c].ROMFileName)
			free(ExtraROMs[c].ROMFileName);

	memcpy(this, &i, sizeof(ElectronConfiguration));

	/* make local copies of any strings - to avoid memory problems later */
	if(ROMPath)
		ROMPath = strdup(ROMPath);
	if(GFXPath)
		GFXPath = strdup(GFXPath);
	c = MAXNUM_EXTRAROMS;
	while(c--)
		if(ExtraROMs[c].ROMFileName)
			ExtraROMs[c].ROMFileName = strdup(ExtraROMs[c].ROMFileName);
}

bool ElectronConfiguration::operator !=(ElectronConfiguration &rvalue)
{
#define Compare(n) 	if(n != rvalue.n) return true;

	Compare(Plus1);
	Compare(FirstByte);
	Compare(FastTape);
	Compare(Autoload);
	Compare(Autoconfigure);
	Compare(MRBMode);
	Compare(Plus3.Enabled);
	Compare(Plus3.Drive1WriteProtect);
	Compare(Plus3.Drive2WriteProtect);
	Compare(Display.AllowOverlay);
	Compare(Display.DisplayMultiplexed);
	Compare(Display.StartFullScreen);
	Compare(Volume);
#undef Compare
#define Compare(sv)\
	if(sv || rvalue.sv)\
	{\
		if(!sv || !rvalue.sv) return true;\
		if(strcmp(sv, rvalue.sv)) return true;\
	}

	Compare(ROMPath);
	Compare(GFXPath);
	int c = MAXNUM_EXTRAROMS;
	while(c--)
		Compare(ExtraROMs[c].ROMFileName);
#undef Compare

	return false;
}

void ElectronConfiguration::Read( BasicConfigurationStore * store )
{
	Plus1 = store->ReadBool( "Plus1", false );
	FirstByte = store->ReadBool( "FirstByte", false );
	FastTape = store->ReadBool( "FastTape", true );
	Autoload = store->ReadBool("Autoload", true );
	Autoconfigure = store->ReadBool("Autoconfigure", true );
	Plus3.Enabled = store->ReadBool( "Plus3", false );
	Plus3.Drive1WriteProtect = store->ReadBool("Drive1WriteProtect", false );
	Plus3.Drive2WriteProtect = store->ReadBool("Drive2WriteProtect", false );
	Display.AllowOverlay = store->ReadBool("AllowOverlay", false );
	Display.DisplayMultiplexed = store->ReadBool("DisplayMultiplexed", true );
	Display.StartFullScreen = store->ReadBool("StartFullScreen", false );
	Volume = store->ReadInt( "Volume", 128 ); if(Volume < 0) Volume = 0; if(Volume > 255) Volume = 255;
	Jim = store->ReadBool("JimEnabled", false);
	PersistentState = store->ReadBool("PersistentState", false);

	switch( store->ReadInt( "SloggerMRB", 0 ) )
	{
		case 0: MRBMode = MRB_OFF; break;
		case 1: MRBMode = MRB_TURBO; break;
		case 2: MRBMode = MRB_SHADOW; break;
		case 3: MRBMode = MRB_4Mhz; break;
	}

	int c = MAXNUM_EXTRAROMS;
	while(c--)
	{
		char settingname[20];
		sprintf(settingname, "Rom%dFileName", c);
		ExtraROMs[c].ROMFileName = store->ReadString(settingname, NULL);
		if(ExtraROMs[c].ROMFileName && !strlen(ExtraROMs[c].ROMFileName))
		{
			free(ExtraROMs[c].ROMFileName);
			ExtraROMs[c].ROMFileName = NULL;
		}
	}
} 

void ElectronConfiguration::Write( BasicConfigurationStore * store )
{
	store -> WriteBool( "Plus1", Plus1 );
	store -> WriteBool( "FirstByte", FirstByte );
	store -> WriteBool( "FastTape", FastTape );
	store -> WriteBool( "Autoload", Autoload);
	store -> WriteBool( "Autoconfigure", Autoconfigure);

	store -> WriteBool( "Plus3", Plus3.Enabled );
	store -> WriteBool( "Drive1WriteProtect", Plus3.Drive1WriteProtect);
	store -> WriteBool( "Drive2WriteProtect", Plus3.Drive2WriteProtect);

	store -> WriteBool( "AllowOverlay", Display.AllowOverlay );
	store -> WriteBool( "DisplayMultiplexed", Display.DisplayMultiplexed);
	store -> WriteBool( "StartFullScreen", Display.StartFullScreen );
	
	store -> WriteBool( "PersistentState", PersistentState);
	store -> WriteBool( "JimOn", Jim);

	store -> WriteInt( "Volume", Volume);

	int sloggerMode;
	switch( MRBMode )
	{
		default:
		case MRB_OFF:
			sloggerMode = 0;
			break;
		case MRB_TURBO:
			sloggerMode = 1;
			break;
		case MRB_SHADOW:
			sloggerMode = 2;
			break;
		case MRB_4Mhz:
			sloggerMode = 3;
			break;
	}

	store -> WriteInt( "SloggerMRB", sloggerMode );

	int c = MAXNUM_EXTRAROMS;
	while(c--)
	{
		char settingname[20];
		sprintf(settingname, "Rom%dFileName", c);
		store->WriteString(settingname, ExtraROMs[c].ROMFileName);
	}
}
