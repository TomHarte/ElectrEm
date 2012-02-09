/*
 *  PosixHostMachine.h
 *  ElectrEm
 *
 *  Created by Ewen Roberts on Mon Jul 19 2004.
 *
 */

class PosixHostMachine : public HostMachine
{
public:
	PosixHostMachine();
	virtual ~PosixHostMachine();
	
	// File Wrangling
	virtual FileDesc *GetFolderContents(char *name);

	// Utility Functions
	virtual void RegisterArgs(int, char *[]);
	virtual void DisplayError(char *fmt, ...);
	
	virtual int MaxPathLength() const;
	virtual char DirectorySeparatorChar() const;

	BasicConfigurationStore * OpenConfigurationStore( const char * name );

protected:
	virtual char *GetExecutablePath();
	
protected:
	char *ExecutablePath;
};