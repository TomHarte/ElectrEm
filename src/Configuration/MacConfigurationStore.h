/*
 *  MacConfigurationStore.h
 *  ElectrEm
 *
 *  Created by Ewen Roberts on Sun Jul 18 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */

#include "ConfigurationStore.h"

class MacConfigurationStore : public BasicConfigurationStore
{
public:
	MacConfigurationStore();
	virtual ~MacConfigurationStore();
	
	virtual void Flush();
	
	virtual bool ReadBool( const char * settingName, bool defaultValue );
	virtual void WriteBool( const char * settingName, bool settingValue );

	virtual int ReadInt( const char * settingName, int defaultValue );
	virtual void WriteInt( const char * settingName, int settingValue );
	
	virtual char * ReadString( const char * settingName, const char * defaultValue );
	virtual void WriteString( const char * settingName, const char * settingValue );
};

