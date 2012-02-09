#ifndef __DISCHELPER_H
#define __DISCHELPER_H

#include "SDL.h"

class CDiscHelper
{
	public :
		bool Setup(Uint16 poly, Uint16 rvalue);
		void Close(void);

		/* CRC related */
		void CRCReset(void);
		void CRCAdd(Uint8 value);
		Uint16 CRCGet(void);

		/* MFM & FM related */
		Uint8 Deflate(Uint16 v);
		Uint16 MFMInflate(Uint8 v, Uint8 clock = 0xff);
		Uint16 FMInflate(Uint8 v, Uint8 clock = 0xff);
		void MFMDeclare(Uint16 v);

	private :
		static Uint16 CRCTable[256], resetval;
		Uint16 CRCValue;
		int LastBit;
};

#endif
