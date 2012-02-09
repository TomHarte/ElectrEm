//
//  CocoaHelpers.h
//  ElectrEm
//
//  Created by Ewen Roberts on Sun Jul 18 2004.
//

class CProcessPool;
class MacHostMachine : public HostMachine
{
public:
	MacHostMachine();
	virtual ~MacHostMachine();	
	virtual void DisplayError(char *fmt, ...);
	virtual void DisplayWarning(char *fmt, ...);
	FileDesc *GetFolderContents( const char *);
	virtual FileSpecs GetSpecs(const char *);
	
	bool ProcessEvent( SDL_Event * evt, CProcessPool * pool );
	virtual int MaxPathLength() const;
	virtual char DirectorySeparatorChar() const;
	
	virtual void ReadConfiguration( BasicConfigurationStore * store );
	BasicConfigurationStore * OpenConfigurationStore( const char * name );
	virtual void CloseConfigurationStore( BasicConfigurationStore * store );
	
	virtual void SaveScreenShot( SDL_Surface * surface );
	
protected:
	virtual char *GetExecutablePath();

protected:
	NSString * resourcesPath;
	NSAutoreleasePool * autoreleasepool;
};
