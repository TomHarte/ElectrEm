/*
 *  PosixHostMachine.cpp
 *  ElectrEm
 *
 *  Created by Ewen Roberts on Mon Jul 19 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */
#include "HostMachine.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <dirent.h>
#include "PosixHostMachine.h"

static PosixHostMachine myHost;

HostMachine * GetHost()
{
	return &myHost;
}

char PosixHostMachine::DirectorySeparatorChar() const
{
	return '/';
}

void PosixHostMachine::DisplayError(char *fmt, ...)
{
	va_list Arguments;
	
	va_start(Arguments, fmt);
	vfprintf(stderr, fmt, Arguments);
	va_end(Arguments);
	fputc('\n', stderr);
}

FileDesc * PosixHostMachine::GetFolderContents(char *name)
{
	FileDesc *Head = NULL, **NPtr = &Head;
	
	DIR * dir = ::opendir( name );
	
	if ( NULL == dir )
	{
		return NULL;
	}
	
	dirent * ent = ::readdir( dir );
	
	while ( NULL != ent )
	{
		*NPtr = new FileDesc;
		(*NPtr)->Name = strdup(ent->d_name);
		(*NPtr)->Type = (ent->d_type == DT_DIR ) ? FileDesc::FD_DIRECTORY : FileDesc::FD_FILE;
		(*NPtr)->Next = NULL;
		NPtr = &(*NPtr)->Next;
		
		ent = ::readdir( dir );
	}
	
	::closedir( dir );
	
	return SortFDList(Head);
}

int PosixHostMachine::MaxPathLength() const
{
	return 1024;
}

BasicConfigurationStore * PosixHostMachine::OpenConfigurationStore( const char * name )
{
	return new ConfigurationStore( name );
}

// Utility Functions
void PosixHostMachine::RegisterArgs(int, char * argv[])
{
	ExecutablePath = argv[0];
}
