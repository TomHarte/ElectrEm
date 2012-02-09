/*
 *  GUIEvents.h
 *  ElectrEm
 *
 *  Created by Ewen Roberts on Sat Jul 10 2004.
 *
 */

#ifndef __GUIEVENTS_H
#define __GUIEVENTS_H

#include "ProcessPoolDebugCodes.h"

/* SDL User events for the GUI Wrapper */

/* Load a UEF file */
#define GUIEVT_LOADFILE			PPDEBUG_GUISTART
/* Pause Emulation */
#define GUIEVT_PAUSE			(PPDEBUG_GUISTART+1)
/* Continue Emulation */
#define GUIEVT_CONTINUE			(PPDEBUG_GUISTART+2)
/* Reset Emulator */
#define GUIEVT_RESET			(PPDEBUG_GUISTART+3)
/* Assign a new configuration to the emulator */
#define GUIEVT_ASSIGNCONFIG		(PPDEBUG_GUISTART+4)
/* Go full screen (note is intended to overlap with PPDEBUG_FSTOGGLE) */
#define GUIEVT_FULLSCREEN		PPDEBUG_FSTOGGLE
/* Copy emulation screen to clipboard */
#define GUIEVT_COPY             (PPDEBUG_GUISTART+5)
/* Load a UEF file */
#define GUIEVT_SAVESTATE		(PPDEBUG_GUISTART+6)
/* Break (issue a break command) */
#define GUIEVT_BREAK			(PPDEBUG_GUISTART+7)
/* type the supplied text */
#define GUIEVT_INSERTTEXT		(PPDEBUG_GUISTART+8)
/* set a file to use for printer output */
#define GUIEVT_SETPRINTERFILE	(PPDEBUG_GUISTART+9)
/* export BASIC as a text file */
#define GUIEVT_EXPORTBASIC		(PPDEBUG_GUISTART+10)
/* import a text file as BASIC */
#define GUIEVT_IMPORTBASIC		(PPDEBUG_GUISTART+11)
/* close printer output */
#define GUIEVT_CLOSEPRINTERFILE	(PPDEBUG_GUISTART+12)
/* force a form feed */
#define GUIEVT_FORMFEED			(PPDEBUG_GUISTART+13)
/* dump the current contents of memory */
#define GUIEVT_DUMPMEMORY		(PPDEBUG_GUISTART+14)

/* disc inserts & ejects, one for each drive. These are assumed sequential by other parts of code! */
#define GUIEVT_INSERTDISC0		(PPDEBUG_GUISTART+15)
#define GUIEVT_INSERTDISC1		(PPDEBUG_GUISTART+16)
#define GUIEVT_EJECTDISC0		(PPDEBUG_GUISTART+17)
#define GUIEVT_EJECTDISC1		(PPDEBUG_GUISTART+18)
#define GUIEVT_INSERTTAPE		(PPDEBUG_GUISTART+19)
#define GUIEVT_EJECTTAPE		(PPDEBUG_GUISTART+20)
#define GUIEVT_REWINDTAPE		(PPDEBUG_GUISTART+21)

#endif
