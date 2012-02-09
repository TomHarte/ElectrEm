/*
 *  MacConfigurationStore.cpp
 *  ElectrEm
 *
 *  Created by Ewen Roberts on Sun Jul 18 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */

#include "MacConfigurationStore.h"
#import <Cocoa/Cocoa.h>

#define preferences [NSUserDefaults standardUserDefaults]

MacConfigurationStore::MacConfigurationStore()
{
}

MacConfigurationStore::~MacConfigurationStore()
{
}

void MacConfigurationStore::Flush()
{
	[preferences synchronize];
}

bool MacConfigurationStore::ReadBool( const char * settingName, bool defaultValue )
{
	NSString * prefName = [[NSString alloc] initWithCString: settingName];
	int val = [preferences boolForKey: prefName];
	return (val == YES );
}

void MacConfigurationStore::WriteBool( const char * settingName, bool settingValue )
{
	NSString * prefName = [[NSString alloc] initWithCString: settingName];
	int v = (settingValue ? YES : NO );
	[preferences setBool:v forKey: prefName ];
}

int MacConfigurationStore::ReadInt( const char * settingName, int defaultValue )
{
	NSString * prefName = [[NSString alloc] initWithCString: settingName];
	return [preferences integerForKey: prefName ];
}

void MacConfigurationStore::WriteInt( const char * settingName, int settingValue )
{
	NSString * prefName = [[NSString alloc] initWithCString: settingName];
	[preferences setInteger:settingValue forKey: prefName ];
}

char * MacConfigurationStore::ReadString( const char * settingName, const char * defaultValue )
{
	NSString * prefName = [[NSString alloc ] initWithCString: settingName ];
	if ( NULL == defaultValue )
	{
		return NULL;
	}
	return strdup( [[ preferences objectForKey: prefName]  cString] );
}

void MacConfigurationStore::WriteString( const char * settingName, const char * settingValue )
{
	NSString * prefName = [NSString stringWithCString: settingName];
	if(settingValue)
		[preferences setObject:[NSString stringWithCString: settingValue] forKey: prefName ];
	else
		[preferences setObject:[NSString stringWithCString: ""] forKey: prefName ];
}

