#ifndef __COMPONENTBASE_H
#define __COMPONENTBASE_H

#include "SDL.h"

#define CYCLENO_ANY	40064

#define IOCTL_NEWTRAPFLAGS		0
#define IOCTL_SETIRQ			1
#define IOCTL_SETNMI			2
#define IOCTL_SETRST			3
#define IOCTL_SETCLOCKRATE		4
#define IOCTL_PAUSE				5
#define IOCTL_UNPAUSE			6
#define IOCTL_SUPERRESET		7
#define IOCTL_RESET				8
#define IOCTL_SETCONFIG			9
#define IOCTL_GETCONFIG			10
#define IOCTL_SETCONFIG_RESET	11
#define IOCTL_GETCONFIG_FINAL	12

/* other IOCtl ranges (there's no consistent logic to these, they are first come, first serve, some subsequently removed...):

		6502:			0x200
		display:		0x300
		processpool:	0x400
		tape:			0x500
		Plus 1:			0x600

*/

class CProcessPool;

/* base class for all machine components */
class CComponentBase
{
	public:
		virtual ~CComponentBase() {};
	
		/* AttachTo, gives CComponentBase's an opportunity to bone up */
		virtual void AttachTo(CProcessPool &, Uint32 id);

		/* read and write functions for caught addresses. Return true if an immediate
		break & Update is required */
		virtual bool Write(Uint16 Addr, Uint32 TimeStamp, Uint8 Data8, Uint32 Data32);
		virtual bool Read(Uint16 Addr, Uint32 TimeStamp, Uint8 &Data8, Uint32 &Data32);

		/* misc. parameter setter / message sender */
		virtual bool IOCtl(Uint32 Control, void *Parameter = NULL, Uint32 TimeStamp = 0);

		/*

		Update function - called to cause the device to progress the requested
		number of (2Mhz) cycles. Should return the number of cycles until next want the
		option of setting CPU control lines without an explicit read or write after the
		current update has finished.

		If Catchup is 'true' then the emulation is currently running too slowly, and any
		components that can should cut their workload

		*/
		virtual Uint32 Update(Uint32 TotalCycles, bool Catchup);

	protected:
		CProcessPool *PPPtr;
		Uint32 PPNum;
};

#endif
