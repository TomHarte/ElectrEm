/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	plus3/helper/helper.cpp
	=======================

	Code for :

	- calculating a 16bit CRC from an arbitrary polynomial and 'processed'
	reload value. This is used principly by various parts of the floppy disc
	subsystem - the WD1770 itself and some of the floppy drives itself in
	cases where CRCs are correct but not explicitly stored.

	- encoding and decoding bytes, possibly with altered clocks, to/from both
	MFM and FM

*/

#include "Helper.h"

Uint16 CDiscHelper::CRCTable[256], CDiscHelper::resetval;

/* setup/close */

bool CDiscHelper::Setup(Uint16 poly, Uint16 rvalue)
{
	/* build CRCTable */
	int c = 256, bc;
	unsigned short crctemp;

	while(c--)
	{
		crctemp = c << 8;
		bc = 8;

		while(bc--)
		{
			if(crctemp&0x8000)
			{
				crctemp = (crctemp << 1) ^ poly;
			}
			else
			{
				crctemp <<= 1;
			}
		}

		CRCTable[c] = crctemp;
	}

	resetval = rvalue;

	return true;
}

void CDiscHelper::Close(void)
{
}

/* CRC bits */

void CDiscHelper::CRCReset(void)
{
	CRCValue = resetval;
}

Uint16 CDiscHelper::CRCGet(void)
{
	return CRCValue;
}

void CDiscHelper::CRCAdd(Uint8 value)
{
	CRCValue = (CRCValue << 8) ^ CRCTable[(CRCValue >> 8)^value];
}

/* MFM/FM bits */

Uint8 CDiscHelper::Deflate(Uint16 v)
{
	return	((v&0x4000) >> 7) |
			((v&0x1000) >> 6) |
			((v&0x0400) >> 5) |
			((v&0x0100) >> 4) |
			((v&0x0040) >> 3) |
			((v&0x0010) >> 2) |
			((v&0x0004) >> 1) |
			(v&0x0001);
}

Uint16 CDiscHelper::MFMInflate(Uint8 v, Uint8 Clock)
{
	Uint16 BuiltValue;

	BuiltValue =	((v&0x80) << 7) |
					((v&0x40) << 6) |
					((v&0x20) << 5) |
					((v&0x10) << 4) |
					((v&0x08) << 3) |
					((v&0x04) << 2) |
					((v&0x02) << 1) |
					(v&0x01) |

					((v&0x80) ? 0 : (LastBit	? 0 : ((Clock&0x80) << 8)) ) |
					((v&0x40) ? 0 : ((v&0x80)	? 0 : ((Clock&0x40) << 7)) ) |
					((v&0x20) ? 0 : ((v&0x40)	? 0 : ((Clock&0x20) << 6)) ) |
					((v&0x10) ? 0 : ((v&0x20)	? 0 : ((Clock&0x10) << 5)) ) |
					((v&0x08) ? 0 : ((v&0x10)	? 0 : ((Clock&0x08) << 4)) ) |
					((v&0x04) ? 0 : ((v&0x08)	? 0 : ((Clock&0x04) << 3)) ) |
					((v&0x02) ? 0 : ((v&0x04)	? 0 : ((Clock&0x02) << 2)) ) |
					((v&0x01) ? 0 : ((v&0x02)	? 0 : ((Clock&0x01) << 1)) );

	LastBit = v&1;

	return BuiltValue;
}

void CDiscHelper::MFMDeclare(Uint16 v)
{
	LastBit = v&1;
}

Uint16 CDiscHelper::FMInflate(Uint8 v, Uint8 Clock)
{
	return	((v&0x80) << 7) |
			((v&0x40) << 6) |
			((v&0x20) << 5) |
			((v&0x10) << 4) |
			((v&0x08) << 3) |
			((v&0x04) << 2) |
			((v&0x02) << 1) |
			(v&0x01) |

			((Clock&0x80) << 8) |
			((Clock&0x40) << 7) |
			((Clock&0x20) << 6) |
			((Clock&0x10) << 5) |
			((Clock&0x08) << 4) |
			((Clock&0x04) << 3) |
			((Clock&0x02) << 2) |
			((Clock&0x01) << 1);
}
