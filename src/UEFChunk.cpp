/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	UEFChunk.cpp
	============

	Chunk level parts of (second version) UEF class

*/
#include "UEF.h"
#include <stdarg.h>
#include <malloc.h>
#include <string.h>

CUEFChunk::CUEFChunk(gzFile input, char *name, bool wrte)
{
	memory = NULL;
	next = NULL;
	Create(input, name, wrte);
	refcount = 0;
}

CUEFChunk::~CUEFChunk(void)
{
}

int CUEFChunk::GetRefCount(void)
{
	return refcount;
}

int CUEFChunk::Read(void *buf, int numbytes)
{
	if(memory)
	{
		if((numbytes+readptr) >= length)
			numbytes = length-readptr;

		memcpy(buf, &memory[readptr], numbytes);

		readptr += numbytes;

		return numbytes;
	}
	else
		return 0;
}

int CUEFChunk::Seek(Uint32 *variable, long offset, int mode)
{
	if(memory)
	{
		switch(mode)
		{
			case SEEK_SET :		(*variable) = offset;			break;
			case SEEK_CUR :		(*variable) += offset;			break;
			case SEEK_END :		(*variable) = length - offset;	break;
		}

		if((*variable) > length) (*variable) = length;
		if((*variable) < 0) (*variable) = 0;

		return readptr;
	}
	else
		return -1;
}

int CUEFChunk::ReadSeek(long offset, int mode)
{
	return Seek(&readptr, offset, mode);
}

int CUEFChunk::GetC(void)
{
	if(readptr == length || !memory) return UEF_EOF;
	return memory[readptr++];
}

Uint16 CUEFChunk::Get16()
{
	Uint16 r;
	r = GetC();
	r |= GetC() << 8;
	return r;
}

Uint32 CUEFChunk::Get32()
{
	Uint32 r;
	r = GetC();
	r |= GetC() << 8;
	r |= GetC() << 16;
	r |= GetC() << 24;
	return r;
}

char *CUEFChunk::GetString()
{
	char *RetBuf = (char *)malloc(length-readptr), *Ptr = RetBuf;
	while(1)
	{
		*Ptr = GetC();
		if(!*Ptr || EOC())
			break;
		Ptr++;
	}

	return RetBuf;
}

#include <math.h>

float CUEFChunk::GetFloat()
{
	/* IEEE format in intel encoding */
	Uint8 Float[4];

	Float[0] = GetC();
	Float[1] = GetC();
	Float[2] = GetC();
	Float[3] = GetC();

	/* decode mantissa */
	Uint32 Mantissa;
	Mantissa = Float[0] | (Float[1] << 8) | ((Float[2]&0x7f)|0x80) << 16;

	float Result = (float)Mantissa;
	Result = (float)ldexp(Result, -23);

	/* decode exponent */
	int Exponent;
	Exponent = ((Float[2]&0x80) >> 7) | (Float[3]&0x7f) << 1;
	Exponent -= 127;
	Result = (float)ldexp(Result, Exponent);

	/* flip sign if necessary */
	if(Float[3]&0x80)
		Result = -Result;
	
	return Result;
}

int CUEFChunk::Write(const void *buf, int numbytes)
{
	if(!write) return -1;

	if(memory)
	{
		dirty = true;

		if(writeptr+numbytes > memorylen)
		{
			/*

			when realloc'ing memory, the newsize is rounded up to the nearest
			256 bytes. This size is fairly optimal for minimising the required
			number of reallocs with respect to the average sort of data sent
			to a UEF file

			*/
			memorylen = (writeptr+numbytes);
			memorylen += 256 - (memorylen&255);
			memory = (Uint8 *)realloc(memory, memorylen);
		}

		memcpy(&memory[writeptr], buf, numbytes);
		writeptr += numbytes;

		if(writeptr > length) length = writeptr;

		return numbytes;
	}

	return -1;
}

int CUEFChunk::WriteSeek(long offset, int mode)
{
	return write ? Seek(&writeptr, offset, mode) : -1;
}

void CUEFChunk::PutC(int value)
{
	/*

	PutC would be a little more complex than GetC, as it has to be checked
	that enough memory is allocated, and if not stuff has to be realloc'd and
	adjusted, so its easier just to call Write and not worry about it here

	*/
	Uint8 val = value;
	Write(&val, 1);
}

void CUEFChunk::Put16(Uint16 value)
{
	PutC(value&0xff);
	PutC(value >> 8);
}

void CUEFChunk::Put32(Uint32 value)
{
	PutC(value&0xff);
	PutC((value >> 8)&0xff);
	PutC((value >> 16)&0xff);
	PutC(value >> 24);
}

void CUEFChunk::PutFloat(float value)
{
	Uint8 Float[4];

	/* sign bit */
	if(value < 0)
	{
		value = -value;
		Float[3] = 0x80;
	}
	else
		Float[3] = 0;

	/* decode mantissa and exponent */
	float mantissa;
	int exponent;
	mantissa = (float)frexp(value, &exponent);
	exponent += 126;

	/* store mantissa */
	Uint32 IMantissa = (Uint32)(mantissa * (1 << 24));
	Float[0] = (Uint8)(IMantissa&0xff);
	Float[1] = (Uint8)((IMantissa >> 8)&0xff);
	Float[2] = (Uint8)((IMantissa >> 16)&0x7f);

	/* store exponent */
	Float[3] |= exponent >> 1;
	Float[2] |= (exponent&1) << 7;

	PutC(Float[0]);
	PutC(Float[1]);
	PutC(Float[2]);
	PutC(Float[3]);
}

void CUEFChunk::PutString(char *str)
{
	while(*str)
	{
		PutC(*str);
		str++;
	}
}


Uint16 CUEFChunk::GetId(void)
{
	return id;
}

Uint32 CUEFChunk::GetLength(void)
{
	return length;
}

bool CUEFChunk::EOC(void)
{
	return (readptr == length);
}

void CUEFChunk::SetId(Uint16 newid)
{
	if(write)
	{
		id = newid;
		dirty = true;
	}
}

void CUEFChunk::SetLength(Uint32 newlength)
{
	if(write)
	{
		length = newlength;
		dirty = true;
	}
}

bool CUEFChunk::HasChanged(void)
{
	return dirty;
}

bool CUEFChunk::Write(gzFile f, CUEFFile *owner)
{
	/* output the chunk to the specified file */
	bool active = true;

	if(!memory)
	{
		active = false;
		Enable(owner);
	}

	gzputc(f, id&255);
	gzputc(f, id>>8);

	gzputc(f, (length & 0x000000ff) >> 0);
	gzputc(f, (length & 0x0000ff00) >> 8);
	gzputc(f, (length & 0x00ff0000) >> 16);
	gzputc(f, (length & 0xff000000) >> 24);

	gzwrite(f, memory, length);

	if(!active) Disable();

	return true;
}

bool CUEFChunk::Create(gzFile input, char *name, bool wrte)
{
	write = wrte;

	if(name)
	{
		/* store and setup necessary parts */
		fname = strdup(name);
		dirty = false;

		/* retrieve vital information from gzFile - offset, chunk id and length */
		id = gzgetc(input);
		id |= ((int)gzgetc(input)) << 8;

		length = gzgetc(input);
		length |= ((int)gzgetc(input)) << 8;
		length |= ((int)gzgetc(input)) << 16;
		length |= ((int)gzgetc(input)) << 24;

		if(gzeof(input)) {id = 0xffff; length = 0; return false;}

		/* advance file ptr to next chunk & return */
		offset = gzseek(input, length, SEEK_CUR) - length;

		return true;
	}
	else
	{
		id = 0; length = 0;
		dirty = true;
		fname = NULL;

		/* allocate 256 bytes initial maximum size */

		memory = (Uint8 *)malloc(256);
		memorylen = 256;

		return true;
	}

	return false;
}

void CUEFChunk::Kill(void)
{
	if(fname)
	{
		free(fname);
		fname = NULL;
	}
}

int CUEFChunk::Enable(CUEFFile *owner)
{
	readptr = writeptr = 0;

	if(!memory)
	{
		if(! (memory = (Uint8 *)malloc(length)) ) return false;
		memorylen = length;

		gzFile tmp = owner->GetFile(fname);

		if(tmp)
		{
			gzseek(tmp, offset, SEEK_SET);
			gzread(tmp, memory, length);
		}
		else
		{
			free(memory);
			memory = NULL;
		}
	}
	refcount++;

	return refcount;
}

int CUEFChunk::Disable(void)
{
	/*

	if no changes were made, safe to just free memory and forget about it

	*/

	if(!refcount) return refcount;

	refcount--;

	if(!refcount)
	{
		if(!dirty)
		{
			if(memory)
			{
				free(memory);
				memory = NULL;
			}
		}
	}

	return refcount;
}
