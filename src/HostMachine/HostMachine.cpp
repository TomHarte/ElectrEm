/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	FileHelper.cpp
	==============

	Numerous small things that interface between ElectrEm and your OS's
	native file functionality. Most notably contains the filename resolver.

*/

#include "HostMachine.h"
#include "../Configuration/ConfigurationStore.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
class UEFList
{
	public:
		UEFList(UEFList *next, CUEFFile *file, const char *name);
		~UEFList();

		CUEFFile *File;
		char *Name;
		UEFList *Next;
		int References;

} *UEFHead = NULL;

UEFList::UEFList(UEFList *next, CUEFFile *file, const char *name)
{
	Name = strdup(name);
	File = file;
	Next = next;
	References = 1;
}

UEFList::~UEFList()
{
	free(Name);
	if(File)
	{
		File->Close();
		delete File;
	}
}

/* not exported, quicksort of directory lists */

/* false if a should come before b */
bool Higher(FileDesc *a, FileDesc *b)
{
	if((a->Type == FileDesc::FD_DIRECTORY) && (b->Type == FileDesc::FD_FILE)) return false;
	if((b->Type == FileDesc::FD_DIRECTORY) && (a->Type == FileDesc::FD_FILE)) return true;

	/* name comparison */
	char *Ptr1, *Ptr2;

	Ptr1 = a->Name;
	Ptr2 = b->Name;
	while(*Ptr1 && *Ptr2)
	{
		if(tolower(*Ptr1) != tolower(*Ptr2))
			return tolower(*Ptr1) > tolower(*Ptr2);

		Ptr1++;
		Ptr2++;
	}

	/* if we get here, there was no difference for as long as names continued */
	return true;
}

FileDesc *HostMachine::SortFDList(FileDesc *i)
{
	if(!i) return i;

	/* count */
	int c = 0;
	FileDesc *ptr = i;
	while(ptr)
	{
		c++;
		ptr = ptr->Next;
	}

	if(c == 1)
		return i;

	if(c == 2)
	{
		/* just consider order, and return */
		if(!Higher(i, i->Next))
			return i;

		/* flip around */
		ptr = i;
		i = i->Next;
		i->Next = ptr;
		i->Next->Next = NULL;
		return i;
	}

	/* more than 2, so pick a pivot and recurse */
	FileDesc *pivot = i, *Less = NULL, *More = NULL;
	i = i->Next;
	while(i)
	{
		FileDesc *N = i->Next;

		if(Higher(i, pivot))
		{
			i->Next = More;
			More = i;
		}
		else
		{
			i->Next = Less;
			Less = i;
		}

		i = N;
	}

	Less = SortFDList(Less);
	More = SortFDList(More);

	/* re-integrate list */
	if(ptr = Less)
	{
		while(ptr->Next)
			ptr = ptr->Next;
		ptr->Next = pivot;
	}
	else
		Less = pivot;
	pivot->Next = More;

	return Less;
}

HostMachine::HostMachine()
{
	ROMPath = NULL;
	GFXPath = NULL;
}

HostMachine::~HostMachine()
{
	while(UEFHead)
	{
		UEFList *Next = UEFHead->Next;
		delete UEFHead;
		UEFHead = Next;
	}
	if(ROMPath) {free(ROMPath); ROMPath = NULL;}
	if(GFXPath) {free(GFXPath); GFXPath = NULL;}
}

void HostMachine::FreeFolderContents(FileDesc * f)
{
	FileDesc *Next;
	while(f)
	{
		Next = f->Next;
		free(f->Name);
		delete f;
		f = Next;
	}
}

void HostMachine::FullScreen( bool bFullScreen )
{
}

CUEFChunkSelector * HostMachine::GetUEFSelector(const char *name, Uint16 version)
{
	if(!strlen(name)) return NULL;

	/* consider: is file open already? */
	UEFList *It = UEFHead;
	while(It)
	{
		if(!strcmp(It->Name, name))
		{
			It->References++;
			return It->File->GetSelectorPtr();
		}
		
		It = It->Next;
	}
	
	/* if not, open it */
	CUEFFile *Newfile = new CUEFFile;

	char *ResolvedName = ResolveFileName(name);
	if(Newfile->Open(ResolvedName, version, "rw"))
	{
		delete[] ResolvedName;
		UEFHead = new UEFList(UEFHead, Newfile, name);
		return UEFHead->File->GetSelectorPtr();
	}
	else
		if(Newfile->Open(ResolvedName, version, "w"))
		{
			delete[] ResolvedName;
			UEFHead = new UEFList(UEFHead, Newfile, name);
			return UEFHead->File->GetSelectorPtr();
		}
	else
		return NULL;
}

void HostMachine::ReleaseUEFSelector(CUEFChunkSelector * c)
{
	/* find connected file */
	UEFList **It = &UEFHead;
	while(*It)
	{
		if(c->GetFather() == (*It)->File)
		{
			c->GetFather()->ReleaseSelectorPtr(c);
			
			(*It)->References--;
			if(!(*It)->References)
			{
				UEFList *Next = (*It)->Next;
				delete *It;
				*It = Next;
			}
			
			return;
		}
		
		It = &(*It)->Next;
	}
}

void HostMachine::RegisterPath( const char *name, const char *path)
{
	if(!strcmp(name, "%ROMPATH%"))
	{
		if(ROMPath) free(ROMPath);
		if(path) ROMPath = strdup(path);
	}
	if(!strcmp(name, "%GFXPATH%"))
	{
		if(GFXPath) free(GFXPath);
		if(path) GFXPath = strdup(path);
	}
}

#ifdef WIN32
#include <windows.h>
#include "SDL_syswm.h"
#endif

char *HostMachine::ResolveFileName(const char *istr)
{
	if(!istr) return NULL;

	char *RetBuf = new char[MaxPathLength()], *RBPtr;
	RBPtr = RetBuf;

	const char *start, *end;
	char * TempStr = new char[MaxPathLength()];
	int CopyOffset;
	start = istr;
	do
	{
		end = start;
		while(*end != '/' && *end) end++;

		memcpy(TempStr, start, end-start);
		TempStr[end-start] = '\0';
		bool Handled = false;
		CopyOffset = 0;

		if(!strcmp(TempStr, "%ROMPATH%") && ROMPath)
		{
			Handled = true;
			memcpy(RBPtr, ROMPath, strlen(ROMPath)+1);
			RBPtr += strlen(ROMPath);
		}

		if(!strcmp(TempStr, "%GFXPATH%") && GFXPath)
		{
			Handled = true;
			memcpy(RBPtr, GFXPath, strlen(GFXPath)+1);
			RBPtr += strlen(GFXPath);
		}

		if(	!strcmp(TempStr, "%EXECPATH%")	|| 
			(!strcmp(TempStr, "%ROMPATH%") && !ROMPath) ||
			(!strcmp(TempStr, "%GFXPATH%") && !GFXPath)
			)
		{
			Handled = true;
			char *T = GetExecutablePath();
			memcpy(RBPtr, T, strlen(T)+1);
			RBPtr += strlen(T);
			delete[] T;
		}

		if(	!strcmp(TempStr, "%HOMEPATH%")	)
		{
#ifndef WIN32
			Handled = true;
			char *T = getenv("HOME");
			memcpy(RBPtr, T, strlen(T)+1);
			RBPtr += strlen(RBPtr);
#else
			Handled = true;
			char *T = getenv("APPDATA");
			if(T)
			{
				memcpy(RBPtr, T, strlen(T)+1);
				RBPtr += strlen(RBPtr);
			}
			else
			{
				T = GetExecutablePath();
				memcpy(RBPtr, T, strlen(T)+1);
				RBPtr += strlen(T);
				delete[] T;
			}
#endif
		}


		if( !strncmp(TempStr, "%DOT%", 5) )
		{
#ifndef WIN32
			RBPtr[0] = '.';
			RBPtr[1] = '\0';
			RBPtr ++;
#endif
			CopyOffset = 5;
		}

		if(!Handled && CopyOffset < (end-start))
		{
			memcpy(RBPtr, &start[CopyOffset], (end-start)-CopyOffset);
			RBPtr += (end-start)-CopyOffset;
		}

		RBPtr[0] = DirectorySeparatorChar();
		RBPtr[1] = '\0';
		RBPtr++;
		start = end+1;
	}
	while(*end);

	delete [] TempStr;

	RBPtr--;
	*RBPtr = '\0';
	return RetBuf;
}

bool HostMachine::ExtensionIncluded(char *Name, char **Extensions)
{
	while(*Extensions)
	{
		/* run from start to end, checking every . */
		char *P = Name;
		
		while(1)
		{
			/* find next . (or NULL) */
			while(*P && (*P != '.'))
				P++;
			
			if(!*P) break;
			
			/* if a dot, do compare */
			P++;
			
			/* compare */
			char *CP = *Extensions;
			
			while(*P && *CP)
			{
				if(*CP == '\a') return true;
				if(tolower(*CP) != tolower(*P)) break;
				
				CP++; P++;
			}
			if(!*P && !*CP) return true;
			if(!*P && *CP == '\a') return true;
		}
		
		Extensions++;
	}
	return false;
}

char * HostMachine::GetExecutablePath()
{
	char *NewBuf = new char[MaxPathLength()];
	strcpy(NewBuf, ExecutablePath);
	
	//	fprintf(stderr, "Trimming from %s\n", NewBuf);
	
	char *IPtr = NewBuf + strlen(NewBuf);
	while(*IPtr != '/') IPtr--;
	*IPtr = '\0';
	
	//	fprintf(stderr, "%s\n", NewBuf);
	
	return NewBuf;
}

#ifndef NO_SDL
bool HostMachine::ProcessEvent( SDL_Event * evt, CProcessPool * pool )
{
	return false;
}

void HostMachine::SetupUI( CProcessPool * pool, BasicConfigurationStore * store )
{
}

#endif

// Utility Functions
void HostMachine::RegisterArgs(int, char *[])
{
}

#ifndef HOSTMACHINE_FILEONLY
// Configuration Functions
void HostMachine::ReadConfiguration( BasicConfigurationStore * store )
{
	/* check out any device configuration options from config file */
	char *t = store->ReadString( "ROMPath", "%EXECPATH%/roms" );
	if(t)
	{
		ROMPath = ResolveFileName(t);
		free(t);
	}

	t = store->ReadString( "GFXPath", NULL );
	if(t)
	{
		GFXPath = ResolveFileName(t);
		free(t);
	}
}
#endif

void HostMachine::SaveScreenShot( SDL_Surface * buffer )
{
	SDL_FreeSurface( buffer );
}
