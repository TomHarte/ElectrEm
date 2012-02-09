#ifndef __UEF_H
#define __UEF_H

#include "zlib.h"
#ifndef NO_SDL
#include "SDL.h"
#else

#ifdef WIN32
typedef unsigned __int16 Uint16;
typedef signed __int16 Sint16;
typedef unsigned __int32 Uint32;
typedef signed __int32 Sint32;
typedef unsigned __int8 Uint8;
typedef signed __int8 Sint8;
typedef unsigned __int64 Uint64;
#endif

#endif

#define UEF_EOC -1
#define UEF_EOF -1

class CUEFFile;
class CUEFChunkSelector;

class CUEFChunk
{
	public :
		CUEFChunk(gzFile input, char *name, bool wrte);
		~CUEFChunk(void);

		int Read(void *buf, int numbytes);
		int ReadSeek(long offset, int mode);
		int GetC();
		Uint16 Get16();
		Uint32 Get32();
		float GetFloat();
		char *GetString(); /* returns a local, malloc'd copy */

		int Write(const void *buf, int numbytes);
		int WriteSeek(long offset, int mode);
		void PutC(int value);
		void Put16(Uint16);
		void Put32(Uint32);
		void PutFloat(float);
		void PutString(char *);

		Uint16 GetId(void);
		Uint32 GetLength(void);
		bool EOC(void);

		void SetId(Uint16 id);
		void SetLength(Uint32 length);

	private :

		bool Create(gzFile input, char *name, bool wrte);
		bool HasChanged(void);
		bool Write(gzFile f, CUEFFile *owner);
		void Kill(void);

		int Enable(CUEFFile *owner);
		int Disable(void);
		int refcount;
		int GetRefCount(void);

		CUEFChunk *next, *last;
		friend class CUEFChunkSelector;
		friend class CUEFFile;

		/*

		generalised seek

		*/

		int Seek(Uint32 *variable, long offset, int mode);

		/*

		memory is used to store the chunk contents while this chunk is active,
		or until file write time if changes are made

		*/

		Uint8 *memory;
		Uint32 memorylen;
		bool dirty;

		/*

		read and write pointers

		*/

		Uint32 readptr, writeptr;
		bool write;

		/*

		a file block is unique by virtue of the file its in, and its start
		location (in bytes offset) within that file

		*/

		char *fname;
		int offset;

		/*

		just a store of this chunk's id and length

		*/

		Uint16 id;
		Uint32 length;
};

class CUEFFile
{
	public :
		CUEFFile(void);
		~CUEFFile(void);

		bool Open(char *name, Uint16 version, char *mode);
		void Close(void);
		char *GetFileName();	//do not free the returned filename!

		Uint16 GetVersion(void);
		CUEFChunkSelector *GetSelectorPtr(void);
		void ReleaseSelectorPtr(CUEFChunkSelector *selt);

		bool HasFile(void);

	private :
		void BuildFileChain(gzFile file, char *fname);
		void Killing(CUEFChunk *chunk);

		gzFile GetFile(char *name);
		friend class CUEFChunk;
		friend class CUEFChunkSelector;

		char *fname, *tempname;
		CUEFChunk *list;

		bool read, write;
		Uint16 newversion, oldversion;

		gzFile cinput;
		char *cname;
};

class CUEFChunkSelector
{
	public :
		CUEFChunkSelector(CUEFFile *initialiser, CUEFChunk *first, bool wrte);
		~CUEFChunkSelector(void);

		char *GetFileName(); //do not free the returned filename!

		int GetOffset(void);
		bool Seek(long offset, int mode);
		bool FindId(Uint16 id);
		bool FindIdMajor(Uint8 id);
		void Reset(void);

		CUEFChunk *CurrentChunk(void);

		CUEFChunk *GetChunkPtr(void);
		CUEFChunk *EstablishChunk(void);
		void ReleaseChunkPtr(CUEFChunk *chunk);

		bool RemoveChunk(CUEFChunk *chunk);
		bool RemoveCurrentChunk(void);

		bool OverRan(void);
		void ResetOverRan(void);

		CUEFFile *GetFather(void);

	private :

		bool Find(Uint16 id, int shifter);
		void ReleaseCurrent(void);

		CUEFChunk *list, *current;
		CUEFFile *father;
		bool justopen, borrowed, overran, write, enabled;
};

#endif
