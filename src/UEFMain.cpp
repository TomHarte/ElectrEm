/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This file at some time additionally worked upon by Paul Boddie

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	UEFMain.cpp
	===========

	Contains UEF class things related to the CUEFFile and
	CUEFChunkSelector classes

*/
#include "UEF.h"
#include <string.h>
#include <malloc.h>
#include <stdio.h>

#ifdef POSIX
#include <unistd.h>
#endif

#ifdef WIN32
#include <io.h>
#define access(x, y) _access(x, y)
#define R_OK 4
#define W_OK 2
#endif

/* CUEFFile class */

CUEFFile::CUEFFile(void)
{
	list = NULL;
	cinput = NULL;
}

CUEFFile::~CUEFFile(void)
{
	Close();
}

/*

	CUEFFile::BuildFileChain - adds the chunks within 'file' (of name
	'fname') to the end of the chunk list

*/
void CUEFFile::BuildFileChain(gzFile file, char *fname)
{
	CUEFChunk *newest, *last, *current;

	/* find end of current chunk chain */
	if(current = list)
	{
		while(current->next != list)
			current = current->next;
	}

	/* load in new chunk information */
	while(!gzeof(file))
	{
		newest = new CUEFChunk(file, fname, write);

		if(!current)
		{
			list = current = newest;
			current->next = NULL;
		}
		else
		{
			last = current;
			current->next = newest;
			current = current->next;
			current->last = last;
		}
	}

	/* relink end of chain to start */
	current->next = list;
	list->last = current;
}

char *CUEFFile::GetFileName()
{
	return fname;
}

bool CUEFFile::Open(char *name, Uint16 version, char *mode)
{
	/*

	determine whether write mode is enabled. Only supported mode strings are
	"w" or "rw", where reading is assumed, and writing is enabled if 'w'
	appears in either of the first two positions

	*/
	Close();

	read = write = false;

	while(*mode)
	{
		switch(*mode)
		{
			default : return false;

			case 'r' : read = true; break;
			case 'w' : write = true; break;
		}

		mode++;
	}

	if(access(name, (read ? R_OK : 0) | (write ? W_OK : 0) ))
	{
		if(read) return false;
	}

	oldversion = newversion = version;
	fname = strdup(name);
	cname = NULL;
	cinput = NULL;

	/*

	should really do something intelligent here to determine the new
	'tempname'

	*/
	if(strcmp("_____tmp.uef", name))
		tempname = strdup("_____tmp.uef");
	else
		tempname = strdup("____tmp_.uef");

	/*

	if read is required, open the named file for input

	*/

	if(read)
	{
		gzFile input;

		if(!(input = gzopen(fname, "rb")))
			return false;

		/*

		read and check the "UEF File!" UEF identifier string

		*/

		char ueftext[10];
		gzread(input, ueftext, 10);

		if(strcmp(ueftext, "UEF File!"))
		{
			gzclose(input);
			return false;
		}

		/*

		check the file version is not greater than that specified, if that
		specified is non-zero

		*/

		Uint8 high, low;

		low = gzgetc(input);
		high = gzgetc(input);
		oldversion = low | ((int)high << 8);

		if(version)
		{
			if( (high != (version >> 8)) || (low > (version&255)) )
			{
				gzclose(input);
				return false;
			}
		}
		else
		{
			newversion = oldversion;
		}

		/*

		if we've got this far, it is indeed an understood UEF file that has
		been named. So build the 'list' chunk list from that, following
		include chunks where necessary

		*/

		BuildFileChain(input, fname);
		gzclose(input);
	}
	else
	{
		/*

		if this UEF is just for writing, create an empty first chunk

		*/

		list = new CUEFChunk(NULL, NULL, write);
		list->next = list->last = list;
	}

	return true;
}

void CUEFFile::Killing(CUEFChunk *chunk)
{
	if(chunk == list) list = list->next;
}

CUEFChunkSelector *CUEFFile::GetSelectorPtr(void)
{
	CUEFChunkSelector *result;

	result = new CUEFChunkSelector(this, list, write);

	return result;
}

void CUEFFile::ReleaseSelectorPtr(CUEFChunkSelector *selt)
{
	delete selt;
}

Uint16 CUEFFile::GetVersion(void)
{
	return (list != NULL) ? oldversion : 0;
}

#define BATCH_SIZE 65536

void move(char *src, char *dst)
{
	/*

	does a file move, in 'BATCH_SIZE' byte length batches. Using
	'rename' won't work since it isn't defined to work across file systems

	BATCH_SIZE can be so small because most UEF files aren't exactly enormous,
	so stack considerations are more pressing

	*/

	FILE *srcfile, *dstfile;
	Uint8 buf[BATCH_SIZE];
	int len;

	srcfile = fopen(src, "rb");
	dstfile = fopen(dst, "wb");

	if(!dstfile || !srcfile)
	{
		if(srcfile) fclose(srcfile);
		if(dstfile) fclose(dstfile);

		return;
	}

	do
	{
		len = fread(buf, sizeof(Uint8), BATCH_SIZE, srcfile);
		fwrite(buf, sizeof(Uint8), len, dstfile);
	}
	while(len == BATCH_SIZE);

	fclose(srcfile);
	fclose(dstfile);

	unlink(src);
}

void CUEFFile::Close(void)
{
	/* check if this CUEFFile actually has any data */
	CUEFChunk *current;

	if(current = list)
	{
		CUEFChunk *next;

		/* force a break in the chain at the end*/

		list->last->next = NULL;

		/* check if changes need to be considered */

		if(write)
		{
			write = false;

			current = list;
			while(current)
			{
				if(current->HasChanged()) write = true;
				current = current->next;
			}
		}

		if(write)
		{
			gzFile output;

			if(output = gzopen(tempname, "wb9"))
			{
				gzprintf(output, "UEF File!"); gzputc(output, 0);

				gzputc(output, newversion&255);
				gzputc(output, newversion >> 8);

				current = list;
				while(current)
				{
					current->Write(output, this);
					current = current->next;
				}

				gzclose(output);
			}
		}

		if(cname)
		{
			free(cname);
			cname = NULL;
		}

		if(cinput)
		{
			gzclose(cinput);
			cinput = NULL;
		}

		if(write)
			move(tempname, fname);

		/* kill & delete all members of chain until break is found */

		current = list;
		while(current)
		{
			next = current->next;

			current->Kill();
			delete current;

			current = next;
		}

		current = list = NULL;

		if(fname)
		{
			free(fname);
			fname = NULL;
		}

		if(tempname)
		{
			free(tempname);
			tempname = NULL;
		}
	}
}

gzFile CUEFFile::GetFile(char *name)
{
	if(!cname || strcmp(name, cname))
	{
		if(cname) free(cname);
		cname = strdup(name);

		if(cinput) gzclose(cinput);
		cinput = gzopen(cname, "rb");
	}

	return cinput;
}

bool CUEFFile::HasFile(void)
{
	return (list != NULL) ? true : false;
}

/* CUEFChunkSelector class */

CUEFChunkSelector::CUEFChunkSelector(CUEFFile *initialiser, CUEFChunk *first, bool wrte)
{
	list = current = first;
	borrowed = true;
	father = initialiser;
	write = wrte;
	Reset();
}

CUEFChunkSelector::~CUEFChunkSelector(void)
{
}

char *CUEFChunkSelector::GetFileName()
{
	return father->GetFileName();
}

void CUEFChunkSelector::Reset(void)
{
	Seek(0, SEEK_SET);
	justopen = true;
}

void CUEFChunkSelector::ReleaseCurrent(void)
{
	if(list)
	{
		if(!borrowed)
		{
			current->Disable();
		}
		borrowed = false;
	}
}

int CUEFChunkSelector::GetOffset(void)
{
	CUEFChunk *it;
	int c = 0;

	it = list;
	while(it != current)
	{
		it = it->next;
		c++;
	}

	return c;
}

bool CUEFChunkSelector::Seek(long offset, int mode)
{
	if(!list || offset < 0) return false;

	justopen = false;
	ReleaseCurrent();

	switch(mode)
	{
		case SEEK_SET :
			current = list;

		case SEEK_CUR :

			while(offset--)
			{
				current = current->next;
				if(current == list) overran = true;
			}

		break;

		case SEEK_END :
			current = list->last;

			while(offset--)
			{
				current = current->last;
				if(current == list->last) overran = true;
			}
		break;
	}

	enabled = false;
	return true;
//	return (current->Enable(father) > 0) ? true : false;
}

bool CUEFChunkSelector::Find(Uint16 id, int shifter)
{
	if(!list) return false;

	CUEFChunk *orig = current;

	if(justopen && ((current->GetId() >> shifter) == id))
	{
		justopen = false;
		return true;
	}

	justopen = false;
	ReleaseCurrent();

	do
	{
		current = current->next;
		if(current == list) overran = true;
	}
	while( (current != orig) && ((current->GetId() >> shifter) != id) );

	if(		((current->GetId() >> shifter) == id) &&
			!((current == orig))
	)
	{
		enabled = false;

		return true;
	}
	else
	{
		current = orig;
		enabled = false;

		return false;
	}
}

bool CUEFChunkSelector::FindId(Uint16 id)
{
	return Find(id, 0);
}

bool CUEFChunkSelector::FindIdMajor(Uint8 id)
{
	return Find(id, 8);
}

#define CheckEnabled()	\
	if(!enabled)	\
	{	\
		current->Enable(father);\
		enabled = true;\
	}

CUEFChunk *CUEFChunkSelector::CurrentChunk(void)
{
	CheckEnabled();

	return (list != NULL) ? current : NULL;
}

CUEFChunk *CUEFChunkSelector::GetChunkPtr(void)
{
	if(!list) return NULL;

	CheckEnabled();

	borrowed = true;
	return CurrentChunk();
}

void CUEFChunkSelector::ReleaseChunkPtr(CUEFChunk *chunk)
{
	chunk->Disable();
}

CUEFChunk *CUEFChunkSelector::EstablishChunk(void)
{
	if(!list) return NULL;

	CUEFChunk *next;

	next = current->next;
	current->next = new CUEFChunk(NULL, NULL, write);

	/* newly created chunk is 'current->next' */

	current->next->next = next;
	current->next->last = current;
	current->next->next->last = current->next;

	Seek(1, SEEK_CUR);

	return CurrentChunk();
}

void CUEFChunkSelector::ResetOverRan(void)
{
	overran = false;
}

bool CUEFChunkSelector::OverRan(void)
{
	return overran;
}

bool CUEFChunkSelector::RemoveCurrentChunk(void)
{
	return RemoveChunk(current);
}

bool CUEFChunkSelector::RemoveChunk(CUEFChunk *chunk)
{
	if(!list) return false;

	if(	current->GetRefCount()	&&
		! ((current->GetRefCount() == 1) && (chunk == current)) )
	return false;

	father->Killing(chunk);

	if(chunk == current)
	{
		Seek(1, SEEK_CUR);
	}

	chunk->next->last = chunk->last;
	chunk->last->next = chunk->next;

	chunk->Kill();
	delete chunk;

	return true;
}

CUEFFile *CUEFChunkSelector::GetFather(void)
{
	return father;
}
