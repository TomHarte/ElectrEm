/*
 *  ConfigurationStore.cpp
 *  ElectrEm
 *
 *  Created by Ewen Roberts on Sat Jul 17 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */
 
#include "Config.h"
#include "ConfigurationStore.h"
#include <string.h>
#include <stdlib.h>


ConfigurationStore::ConfigurationStore( const char * fileName )
{
	m_storage = new CConfig();
	if ( !m_storage -> Open( fileName ) )
	{
		delete m_storage;
		m_storage = NULL;
	}
}

ConfigurationStore::~ConfigurationStore()
{
	if ( NULL != m_storage )
	{
		delete m_storage;
	}
}

void ConfigurationStore::Flush()
{
	m_storage -> Flush();
}

bool ConfigurationStore::ReadBool( const char * settingName, bool defaultValue )
{
	if ( NULL != m_storage &&
		 m_storage -> FindSetting( settingName, CT_BOOLEAN ) )
	{
		return m_storage -> ReadSetting().r.Bool;
	}
	
	return defaultValue;
}

void ConfigurationStore::WriteBool( const char * settingName, bool settingValue )
{
	if ( NULL == m_storage )
	{
		return;
	}

	ConfigSetting NS;
	NS.Type = CT_BOOLEAN;
	NS.r.Bool = settingValue;
	m_storage -> FindSetting(settingName);
	m_storage -> WriteSetting(NS);
}

int ConfigurationStore::ReadInt( const char * settingName, int defaultValue )
{
	if ( NULL != m_storage && m_storage -> FindSetting(settingName, CT_INTEGER) )
	{
		return m_storage -> ReadSetting().r.Int;
	}
	return defaultValue;
}

void ConfigurationStore::WriteInt( const char * settingName, int settingValue )
{
	if ( NULL == m_storage )
	{
		return;
	}
	
	ConfigSetting NS;
	NS.Type = CT_INTEGER;
	NS.r.Int = settingValue;
	m_storage -> FindSetting( settingName );
	m_storage -> WriteSetting(NS);
}

/* weird MSVC 6 workaround */
#ifndef WIN32
#define _strdup strdup
#endif

char * ConfigurationStore::ReadString( const char * settingName, const char * defaultValue )
{

	if ( NULL != m_storage && m_storage -> FindSetting( settingName, CT_STRING ))
	{
		return _strdup( m_storage -> ReadSetting().String );
	}

	if ( NULL == defaultValue )
	{
		return NULL;
	}

	return strdup( defaultValue );
}

void ConfigurationStore::WriteString( const char * settingName, const char * settingValue )
{
	if ( NULL == m_storage )
	{
		return;
	}
	
	ConfigSetting NS;
	NS.Type = CT_STRING;
	NS.String = strdup( settingValue );
	m_storage -> FindSetting( settingName );
	m_storage -> WriteSetting(NS);
	free( NS.String );
}

