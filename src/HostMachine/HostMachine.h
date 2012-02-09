#ifndef __FILEHELPER_H
#define __FILEHELPER_H

/* UEF helpers - so that completely disparate code can access the same UEFs simultaneously */
#include "../UEF.h"

/* path information functions. Essentially a way around the lack of opendir/closedir
in Windows, and the way that the top level path has to be treated specially */
struct FileDesc
{
	char *Name;
	enum
	{
		FD_FILE, FD_DIRECTORY
	} Type;
	FileDesc *Next;
};

/* file specifications */
#define FS_READONLY		0x01
struct FileSpecs
{
	unsigned int Size;
	Uint32 Stats;
};

// A class that allows platform specific interface methods to be overridden.
// This class is not directly created, rather it is accessed through the GetHost() ->
// accessor function described at the end of this h file.
// A deriving class is provided with an implementation of GetHost() that returns a pointer
// to the class for that Host.

class CProcessPool;
class BasicConfigurationStore;
class HostMachine
{
public:
	HostMachine();
	virtual ~HostMachine();
	
	// File Wrangling
	virtual FileDesc *GetFolderContents(const char *name) = 0;
	virtual void FreeFolderContents(FileDesc *);
	virtual CUEFChunkSelector *GetUEFSelector(const char *name, Uint16 version);
	virtual void ReleaseUEFSelector(CUEFChunkSelector *);

	virtual char *ResolveFileName(const char *);
	virtual void RegisterPath( const char *name, const char *path);
	virtual bool ExtensionIncluded(char *Name, char **Extensions);	

	virtual int MaxPathLength() const = 0;
	virtual char DirectorySeparatorChar() const = 0;

	// Utility Functions
	virtual void RegisterArgs(int, char *[]);
	virtual void DisplayError(char *fmt, ...) = 0;
	virtual void DisplayWarning(char *fmt, ...) = 0;
	virtual FileSpecs GetSpecs(const char *) = 0;

	virtual void FullScreen( bool bFullScreen );
	
	// Useful stuff
	virtual void SaveScreenShot( SDL_Surface * surface );

#ifndef HOSTMACHINE_FILEONLY
	// Configuration Functions
	virtual void ReadConfiguration( BasicConfigurationStore * store );
	// Return a configuration store for the host machine.
	virtual BasicConfigurationStore * OpenConfigurationStore( const char * name ) = 0;
	virtual void CloseConfigurationStore( BasicConfigurationStore * store ) = 0;
#endif


#ifndef NO_SDL
	// Allow the HostMachine object to set up any GUI features
	virtual void SetupUI( CProcessPool * pool, BasicConfigurationStore * store );
	// Allows the host to grab events and respond to them using the process pool.
	// This is intended to be used by external GUIs that want to implement eg their own debugger
	virtual bool ProcessEvent( SDL_Event * evt, CProcessPool * pool );
#endif


protected:
	virtual char *GetExecutablePath();
	virtual FileDesc *SortFDList(FileDesc *i);

protected:
	char * ExecutablePath;
	char * ROMPath;
	char * GFXPath;


};

// Returns an object that implements HostMachine for this host.
extern HostMachine * GetHost();

#endif
