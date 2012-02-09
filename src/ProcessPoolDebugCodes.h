/*
 *  ProcessPoolDebugCodes.h
 *  ElectrEm
 *
 *  Created by Thomas Harte on 02/12/2006.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __PROCESSPOOLDEBUGCODES_H
#define __PROCESSPOOLDEBUGCODES_H

#define PPDEBUG_USERQUIT		0x000
#define PPDEBUG_CYCLESDONE		0x001
#define PPDEBUG_ENDOFFRAME		0x002
#define PPDEBUG_IRQ				0x004
#define PPDEBUG_NMI				0x008
#define PPDEBUG_GUI				0x010
#define PPDEBUG_FSTOGGLE		0x020
#define PPDEBUG_INSTRSDONE		0x040
#define PPDEBUG_SCREENFAILED	0x080
#define PPDEBUG_KILLINSTR		0x100
#define PPDEBUG_UNKNOWNOP		0x101
#define PPDEBUG_OSFAILED		0x200
#define PPDEBUG_BASICFAILED		0x400

#define PPDEBUG_GUISTART		0x500

#endif
