#include "Drive.h"
#include "../../HostMachine/HostMachine.h"

CDriveFDI::CDriveFDI()
{
	FDIPtr = NULL;
	File = NULL;
}

CDriveFDI::~CDriveFDI()
{
	if(FDIPtr)
		fdi2raw_header_free(FDIPtr);
	if(File)
		gzclose(File);
}

int CDriveFDI::GetLine(DriveLine d)
{
	switch(d)
	{
		default: return CDrive::GetLine(d);
		case DL_WPROTECT:  return 1;	/* we can't write FDIs yet */
//		case DL_DDENQUERY: return DoubleDensity ? 1 : 0;
	}
}

bool CDriveFDI::Open(char *name)
{
	File = gzopen(name, "r");
	if(!File) return false;

	FDIPtr = fdi2raw_header(File);
	if(!FDIPtr) return false;

	return false;
}

void CDriveFDI::GetEvent(DriveEvent *) {}
int CDriveFDI::GetTrackLength() { return 0;}
