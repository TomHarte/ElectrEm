/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	DisplayTable.cpp
	================

	Serious display table building code

*/

#include "Display.h"
#include <memory.h>

void CDisplay::RefillGraphicsTables()
{
#define CopyYUV(t, s)\
	PaletteYUV[t][0] = (Uint8)(PaletteColours[s] >> 16);\
	PaletteYUV[t][2] = (Uint8)((PaletteColours[s] >> 8)&0xff);\
	PaletteYUV[t][1] = (Uint8)(PaletteColours[s]&0xff);

	if(DisplayMultiplexed)
		switch(CMode)
		{
			case 0: case 3: case 4: case 6:
				PaletteRGB[0] = PaletteColours[0];
				PaletteRGB[1] = PaletteColours[8];

				CopyYUV(0, 0); CopyYUV(1, 8);
			break;

			case 1: case 5:
				PaletteRGB[0] = PaletteColours[0];
				PaletteRGB[1] = PaletteColours[2];
				PaletteRGB[2] = PaletteColours[8];
				PaletteRGB[3] = PaletteColours[10];

				CopyYUV(0, 0); CopyYUV(1, 2);
				CopyYUV(2, 8); CopyYUV(3, 10);
			break;

			case 2:{
				int c = 16;
				while(c--)
				{
					PaletteRGB[c] = PaletteColours[c];
					CopyYUV(c, c);
				}
			}break;
		}
#undef CopyYUV

	/* NB: optimise this all down to just one switch, soon */
	switch(CMode)
	{
		case 0:
		case 3:
			if(PaletteColours[8] == PaletteColours[0])
			{
				Blank = true;
				BlankColour = PaletteColours[0];
			}
//			else
			{
				Blank = false;

				if(Overlay)
				{
					if(DisplayMultiplexed)
					{
						int c = 256;
						while(c--)
						{
							Uint8 *DTB = &MultiDisplayTables[c << TableShift];

							DTB[YOffs1] = PaletteYUV[c >> 4][0];
							DTB[YOffs2] = PaletteYUV[c&0x0f][0];

							DTB[UOffs] = (PaletteYUV[c >> 4][1] + PaletteYUV[c&0x0f][1]) >> 1;
							DTB[VOffs] = (PaletteYUV[c >> 4][2] + PaletteYUV[c&0x0f][2]) >> 1;
						}
					}
					else
					{
						int c = 256;
						while(c--)
						{
							Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];

							int ic = 4, mc = c;
							while(ic--)
							{
								Uint32 Colour1 = (mc&0x80) ? PaletteColours[8] : PaletteColours[0];
								Uint32 Colour2 = (mc&0x40) ? PaletteColours[8] : PaletteColours[0];

								DTB[YOffs1] = DTB[YOffs1+16] = (Uint8)(Colour1 >> 16);
								DTB[YOffs2] = DTB[YOffs2+16] = (Uint8)(Colour2 >> 16);

								DTB[VOffs] = DTB[VOffs+16] = (Uint8)((((Colour1 >> 8)&0xff) + ((Colour2 >> 8)&0xff)) >> 1);
								DTB[UOffs] = DTB[UOffs+16] = (Uint8)(((Colour1 + Colour2) >> 1)&0xff);

								DTB += 4;
								mc <<= 2;
							}
						}
					}
				}
				else
				{
					int c = 256;

					switch(FrameBuffer->format->BytesPerPixel | (DisplayMultiplexed ? 8 : 0))
					{
						case 9:
							while(c--)
							{
								Uint8 *TPtr = &MultiDisplayTables[c << TableShift];

								TPtr[0] = (Uint8)PaletteColours[c >> 4];
								TPtr[1] = (Uint8)PaletteColours[c&0x0f];
							}
							break;
						break;

						case 1:
							while(c--)
							{
								Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[8] = (c&0x80) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];

								DTB[1] = 
								DTB[9] = (c&0x40) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];

								DTB[2] = 
								DTB[10] = (c&0x20) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];

								DTB[3] = 
								DTB[11] = (c&0x10) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];

								DTB[4] = 
								DTB[12] = (c&0x08) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];

								DTB[5] = 
								DTB[13] = (c&0x04) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];

								DTB[6] = 
								DTB[14] = (c&0x02) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];

								DTB[7] = 
								DTB[15] = (c&0x01) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];
							}
						break;

						case 10:
							while(c--)
							{
								Uint16 *DTB = (Uint16 *)(&MultiDisplayTables[c << TableShift]);

								DTB[0] = (Uint16)PaletteRGB[c >> 4];
								DTB[1] = (Uint16)PaletteRGB[c & 0x0f];
							}
						break;

						case 2:
							while(c--)
							{
								Uint16 *DTB = (Uint16 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[8] = (c&0x80) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[1] = 
								DTB[9] = (c&0x40) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[2] = 
								DTB[10] = (c&0x20) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[3] = 
								DTB[11] = (c&0x10) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[4] = 
								DTB[12] = (c&0x08) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[5] = 
								DTB[13] = (c&0x04) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[6] = 
								DTB[14] = (c&0x02) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[7] = 
								DTB[15] = (c&0x01) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
							}
						break;

						case 11:
							while(c--)
							{
								Uint8 *DTB = &MultiDisplayTables[c << TableShift];

								DTB[0] = ((Uint8 *)&PaletteRGB[c >> 4]) [0];
								DTB[1] = ((Uint8 *)&PaletteRGB[c >> 4]) [1];
								DTB[2] = ((Uint8 *)&PaletteRGB[c >> 4]) [2];

								DTB[3] = ((Uint8 *)&PaletteRGB[c&0x0f]) [0];
								DTB[4] = ((Uint8 *)&PaletteRGB[c&0x0f]) [1];
								DTB[5] = ((Uint8 *)&PaletteRGB[c&0x0f]) [2];
							}
						break;

						case 3:
							while(c--)
							{
								Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];
								memset(DTB, 0, 64);

								*((Uint32 *)&DTB[0]) |=		(c&0x80) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[32]) |=	(c&0x80) ? PaletteColours[8] : PaletteColours[0];

								*((Uint32 *)&DTB[3]) |=		(c&0x40) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[35]) |=	(c&0x40) ? PaletteColours[8] : PaletteColours[0];

								*((Uint32 *)&DTB[6]) |=		(c&0x20) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[38]) |=	(c&0x20) ? PaletteColours[8] : PaletteColours[0];

								*((Uint32 *)&DTB[9]) |=		(c&0x10) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[41]) |=	(c&0x10) ? PaletteColours[8] : PaletteColours[0];

								*((Uint32 *)&DTB[12]) |=	(c&0x08) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[44]) |=	(c&0x08) ? PaletteColours[8] : PaletteColours[0]; 

								*((Uint32 *)&DTB[15]) |=	(c&0x04) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[47]) |=	(c&0x04) ? PaletteColours[8] : PaletteColours[0];

								*((Uint32 *)&DTB[18]) |=	(c&0x02) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[50]) |=	(c&0x02) ? PaletteColours[8] : PaletteColours[0]; 

								*((Uint32 *)&DTB[21]) |=	(c&0x01) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[53]) |=	(c&0x01) ? PaletteColours[8] : PaletteColours[0];
							}
						break;

						case 12:
							while(c--)
							{
								Uint32 *DTB = (Uint32 *)&MultiDisplayTables[c << TableShift];

								DTB[0] = PaletteRGB[c >> 4];
								DTB[1] = PaletteRGB[c & 0x0f];
							}
						break;

						case 4:
							while(c--)
							{
								Uint32 *DTB = (Uint32 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[8] = (c&0x80) ? PaletteColours[8] : PaletteColours[0];
								DTB[1] = 
								DTB[9] = (c&0x40) ? PaletteColours[8] : PaletteColours[0];
								DTB[2] = 
								DTB[10] = (c&0x20) ? PaletteColours[8] : PaletteColours[0];
								DTB[3] = 
								DTB[11] = (c&0x10) ? PaletteColours[8] : PaletteColours[0];
								DTB[4] = 
								DTB[12] = (c&0x08) ? PaletteColours[8] : PaletteColours[0];
								DTB[5] = 
								DTB[13] = (c&0x04) ? PaletteColours[8] : PaletteColours[0];
								DTB[6] = 
								DTB[14] = (c&0x02) ? PaletteColours[8] : PaletteColours[0];
								DTB[7] = 
								DTB[15] = (c&0x01) ? PaletteColours[8] : PaletteColours[0];
							}
						break;
					}
				}
			}
		break;

		case 5:
			if( (PaletteColours[0] == PaletteColours[2]) &&
				(PaletteColours[2] == PaletteColours[8]) &&
				(PaletteColours[8] == PaletteColours[10])
				)
			{
				Blank = true;
				BlankColour = PaletteColours[10];
			}
//			else
			{
				Blank = false;

				if(Overlay)
				{
					if(DisplayMultiplexed)
					{
						int c = 256;
						while(c--)
						{
							Uint8 *DTB = &MultiDisplayTables[c<<TableShift];

							DTB[YOffs1] = DTB[YOffs2] = PaletteYUV[c][0];
							DTB[UOffs] = PaletteYUV[c][1];
							DTB[VOffs] = PaletteYUV[c][2];
							DTB[YOffs1+4] = DTB[YOffs2+4] = PaletteYUV[c][0];
							DTB[UOffs+4] = PaletteYUV[c][1];
							DTB[VOffs+4] = PaletteYUV[c][2];
						}
					}
					else
					{
						Uint32 Aside[4] = {PaletteColours[0], PaletteColours[2], PaletteColours[8], PaletteColours[10]};

						int c = 256;
						while(c--)
						{
							Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];

							int ic = 4, mc = c;
							while(ic--)
							{
								Uint32 Colour = Aside[((mc&0x80) >> 6) | ((mc&0x08) >> 3)];

								DTB[YOffs1] = DTB[YOffs2] =
								DTB[YOffs1+4] = DTB[YOffs2+4] = (Uint8)(Colour >> 16);

								DTB[VOffs] = 
								DTB[VOffs+4] = (Uint8)((Colour >> 8)&0xff);

								DTB[UOffs] =
								DTB[UOffs+4] = (Uint8)(Colour&0xff);

								DTB += 8;
								mc <<= 1;
							}
						}
					}
				}
				else
				{
					Uint32 Aside[4] = {PaletteColours[0], PaletteColours[2], PaletteColours[8], PaletteColours[10]};
					int c = 256;

					switch(FrameBuffer->format->BytesPerPixel | (DisplayMultiplexed ? 8 : 0))
					{
						case 9:
							while(c--)
							{
								Uint8 *DTB = &MultiDisplayTables[c<<TableShift];
								DTB[0] = DTB[1] = DTB[2] = DTB[3] = (Uint8)PaletteColours[c];
							}
						break;

						case 1:
							while(c--)
							{
								Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] =
								DTB[2] = 
								DTB[3] = (Uint8)Aside[((c&0x80) >> 6) | ((c&0x08) >> 3)];

								DTB[4] = 
								DTB[5] =
								DTB[6] = 
								DTB[7] = (Uint8)Aside[((c&0x40) >> 5) | ((c&0x04) >> 2)];

								DTB[8] =
								DTB[9] =
								DTB[10] =
								DTB[11] = (Uint8)Aside[((c&0x20) >> 4) | ((c&0x02) >> 1)];

								DTB[12] = 
								DTB[13] =
								DTB[14] = 
								DTB[15] = (Uint8)Aside[((c&0x10) >> 3) | (c&0x01)];
							}
						break;

						case 10:
							while(c--)
							{
								Uint16 *DTB = (Uint16 *)(&MultiDisplayTables[c << TableShift]);

								DTB[0] = 
								DTB[1] = 
								DTB[2] = 
								DTB[3] = (Uint16)PaletteRGB[c];
							}
						break;

						case 2:
							while(c--)
							{
								Uint16 *DTB = (Uint16 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] =
								DTB[2] = 
								DTB[3] = (Uint16)Aside[((c&0x80) >> 6) | ((c&0x08) >> 3)];

								DTB[4] = 
								DTB[5] =
								DTB[6] = 
								DTB[7] = (Uint16)Aside[((c&0x40) >> 5) | ((c&0x04) >> 2)];

								DTB[8] =
								DTB[9] =
								DTB[10] =
								DTB[11] = (Uint16)Aside[((c&0x20) >> 4) | ((c&0x02) >> 1)];

								DTB[12] = 
								DTB[13] =
								DTB[14] = 
								DTB[15] = (Uint16)Aside[((c&0x10) >> 3) | (c&0x01)];
							}
						break;

						case 11:
							while(c--)
							{
								Uint8 *DTB = &MultiDisplayTables[c << TableShift];

								DTB[0] = DTB[3] = DTB[6] = DTB[9] = ((Uint8 *)&PaletteRGB[c]) [0];
								DTB[1] = DTB[4] = DTB[7] = DTB[10] = ((Uint8 *)&PaletteRGB[c]) [1];
								DTB[2] = DTB[5] = DTB[8] = DTB[11] = ((Uint8 *)&PaletteRGB[c]) [2];
							}
						break;

						case 3:
							while(c--)
							{
								Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];
								Uint32 Colour;
								memset(DTB, 0, 64);

								Colour =  Aside[((c&0x80) >> 6) | ((c&0x08) >> 3)];
								*((Uint32 *)&DTB[0]) |= Colour;
								*((Uint32 *)&DTB[3]) |= Colour;
								*((Uint32 *)&DTB[6]) |= Colour;
								*((Uint32 *)&DTB[9]) |= Colour;

								Colour = Aside[((c&0x40) >> 5) | ((c&0x04) >> 2)];
								*((Uint32 *)&DTB[12]) |= Colour;
								*((Uint32 *)&DTB[15]) |= Colour;
								*((Uint32 *)&DTB[18]) |= Colour;
								*((Uint32 *)&DTB[21]) |= Colour;

								Colour = Aside[((c&0x20) >> 4) | ((c&0x02) >> 1)];
								*((Uint32 *)&DTB[32]) |= Colour;
								*((Uint32 *)&DTB[35]) |= Colour;
								*((Uint32 *)&DTB[38]) |= Colour;
								*((Uint32 *)&DTB[41]) |= Colour;

								Colour = Aside[((c&0x10) >> 3) | (c&0x01)];
								*((Uint32 *)&DTB[44]) |= Colour;
								*((Uint32 *)&DTB[47]) |= Colour;
								*((Uint32 *)&DTB[50]) |= Colour;
								*((Uint32 *)&DTB[53]) |= Colour;
							}
						break;

						case 12:
							while(c--)
							{
								Uint32 *DTB = (Uint32 *)&MultiDisplayTables[c << TableShift];

								DTB[0] = 
								DTB[1] = 
								DTB[2] = 
								DTB[3] = PaletteRGB[c];
							}
						break;

						case 4:
							while(c--)
							{
								Uint32 *DTB = (Uint32 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] =
								DTB[2] = 
								DTB[3] = Aside[((c&0x80) >> 6) | ((c&0x08) >> 3)];

								DTB[4] = 
								DTB[5] =
								DTB[6] = 
								DTB[7] = Aside[((c&0x40) >> 5) | ((c&0x04) >> 2)];

								DTB[8] =
								DTB[9] =
								DTB[10] =
								DTB[11] = Aside[((c&0x20) >> 4) | ((c&0x02) >> 1)];

								DTB[12] = 
								DTB[13] =
								DTB[14] = 
								DTB[15] = Aside[((c&0x10) >> 3) | (c&0x01)];
							}
						break;
					}
				}
			}
		break;

		case 1:
			if( (PaletteColours[0] == PaletteColours[2]) &&
				(PaletteColours[2] == PaletteColours[8]) &&
				(PaletteColours[8] == PaletteColours[10])
				)
			{
				Blank = true;
				BlankColour = PaletteColours[10];
			}
//			else
			{
				Blank = false;

				if(Overlay)
				{
					if(DisplayMultiplexed)
					{
						int c = 256;
						while(c--)
						{
							Uint8 *DTB = &MultiDisplayTables[c << TableShift];

							DTB[YOffs1] = 
							DTB[YOffs2] = PaletteYUV[c][0];

							DTB[UOffs] = PaletteYUV[c][1];
							DTB[VOffs] = PaletteYUV[c][2];
						}
					}
					else
					{
						Uint32 Aside[4] = {PaletteColours[0], PaletteColours[2], PaletteColours[8], PaletteColours[10]};

						int c = 256;
						while(c--)
						{
							Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];

							int ic = 4, mc = c;
							while(ic--)
							{
								Uint32 Colour = Aside[((mc&0x80) >> 6) | ((mc&0x08) >> 3)];

								DTB[YOffs1] = DTB[YOffs2] =
								DTB[YOffs1+16] = DTB[YOffs2+16] = (Uint8)(Colour >> 16);

								DTB[VOffs] = 
								DTB[VOffs+16] = (Uint8)((Colour >> 8)&0xff);

								DTB[UOffs] =
								DTB[UOffs+16] = (Uint8)(Colour&0xff);

								DTB += 4;
								mc <<= 1;
							}
						}
					}
				}
				else
				{
					Uint32 Aside[4] = {PaletteColours[0], PaletteColours[2], PaletteColours[8], PaletteColours[10]};
					int c = 256;

					switch(FrameBuffer->format->BytesPerPixel | (DisplayMultiplexed ? 8 : 0))
					{
						case 9:
							while(c--)
							{
								Uint8 *DTB = &MultiDisplayTables[c<<TableShift];
								DTB[0] = DTB[1] = (Uint8)PaletteColours[c];
							}
						break;

						case 1:
							while(c--)
							{
								Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] =
								DTB[8] = 
								DTB[9] = (Uint8)Aside[((c&0x80) >> 6) | ((c&0x08) >> 3)];

								DTB[2] = 
								DTB[3] =
								DTB[10] = 
								DTB[11] = (Uint8)Aside[((c&0x40) >> 5) | ((c&0x04) >> 2)];

								DTB[4] =
								DTB[5] =
								DTB[12] =
								DTB[13] = (Uint8)Aside[((c&0x20) >> 4) | ((c&0x02) >> 1)];

								DTB[6] = 
								DTB[7] =
								DTB[14] = 
								DTB[15] = (Uint8)Aside[((c&0x10) >> 3) | (c&0x01)];
							}
						break;

						case 10:
							while(c--)
							{
								Uint16 *DTB = (Uint16 *)(&MultiDisplayTables[c << TableShift]);
								DTB[0] = DTB[1] = (Uint16)PaletteRGB[c];
							}
						break;

						case 2:
							while(c--)
							{
								Uint16 *DTB = (Uint16 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] =
								DTB[8] = 
								DTB[9] = (Uint16)Aside[((c&0x80) >> 6) | ((c&0x08) >> 3)];

								DTB[2] = 
								DTB[3] =
								DTB[10] = 
								DTB[11] = (Uint16)Aside[((c&0x40) >> 5) | ((c&0x04) >> 2)];

								DTB[4] =
								DTB[5] =
								DTB[12] =
								DTB[13] = (Uint16)Aside[((c&0x20) >> 4) | ((c&0x02) >> 1)];

								DTB[6] = 
								DTB[7] =
								DTB[14] = 
								DTB[15] = (Uint16)Aside[((c&0x10) >> 3) | (c&0x01)];
							}
						break;

						case 11:
							while(c--)
							{
								Uint8 *DTB = &MultiDisplayTables[c << TableShift];

								DTB[0] = DTB[3] = ((Uint8 *)&PaletteRGB[c]) [0];
								DTB[1] = DTB[4] = ((Uint8 *)&PaletteRGB[c]) [1];
								DTB[2] = DTB[5] = ((Uint8 *)&PaletteRGB[c]) [2];
							}
						break;

						case 3:
							while(c--)
							{
								Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];
								Uint32 Colour;
								memset(DTB, 0, 64);

								Colour =  Aside[((c&0x80) >> 6) | ((c&0x08) >> 3)];
								*((Uint32 *)&DTB[0]) |= Colour;
								*((Uint32 *)&DTB[3]) |= Colour;
								*((Uint32 *)&DTB[32]) |= Colour;
								*((Uint32 *)&DTB[35]) |= Colour;

								Colour = Aside[((c&0x40) >> 5) | ((c&0x04) >> 2)];
								*((Uint32 *)&DTB[6]) |= Colour;
								*((Uint32 *)&DTB[9]) |= Colour;
								*((Uint32 *)&DTB[38]) |= Colour;
								*((Uint32 *)&DTB[41]) |= Colour;

								Colour = Aside[((c&0x20) >> 4) | ((c&0x02) >> 1)];
								*((Uint32 *)&DTB[12]) |= Colour;
								*((Uint32 *)&DTB[15]) |= Colour;
								*((Uint32 *)&DTB[44]) |= Colour;
								*((Uint32 *)&DTB[47]) |= Colour;

								Colour = Aside[((c&0x10) >> 3) | (c&0x01)];
								*((Uint32 *)&DTB[18]) |= Colour;
								*((Uint32 *)&DTB[21]) |= Colour;
								*((Uint32 *)&DTB[50]) |= Colour;
								*((Uint32 *)&DTB[53]) |= Colour;
							}
						break;

						case 12:
							while(c--)
							{
								Uint32 *DTB = (Uint32 *)&MultiDisplayTables[c << TableShift];
								DTB[0] = DTB[1] = PaletteRGB[c];
							}
						break;

						case 4:
							while(c--)
							{
								Uint32 *DTB = (Uint32 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] =
								DTB[8] = 
								DTB[9] = Aside[((c&0x80) >> 6) | ((c&0x08) >> 3)];

								DTB[2] = 
								DTB[3] =
								DTB[10] = 
								DTB[11] = Aside[((c&0x40) >> 5) | ((c&0x04) >> 2)];

								DTB[4] =
								DTB[5] =
								DTB[12] =
								DTB[13] = Aside[((c&0x20) >> 4) | ((c&0x02) >> 1)];

								DTB[6] = 
								DTB[7] =
								DTB[14] = 
								DTB[15] = Aside[((c&0x10) >> 3) | (c&0x01)];
							}
						break;
					}
				}
			}
		break;

		case 2:
			if( (PaletteColours[0] == PaletteColours[1]) &&
				(PaletteColours[1] == PaletteColours[2]) &&
				(PaletteColours[2] == PaletteColours[3]) &&
				(PaletteColours[3] == PaletteColours[4]) &&
				(PaletteColours[4] == PaletteColours[5]) &&
				(PaletteColours[5] == PaletteColours[6]) &&
				(PaletteColours[6] == PaletteColours[7]) &&
				(PaletteColours[7] == PaletteColours[8]) &&
				(PaletteColours[8] == PaletteColours[9]) &&
				(PaletteColours[9] == PaletteColours[10]) &&
				(PaletteColours[10] == PaletteColours[11]) &&
				(PaletteColours[11] == PaletteColours[12]) &&
				(PaletteColours[12] == PaletteColours[13]) &&
				(PaletteColours[13] == PaletteColours[14]) &&
				(PaletteColours[14] == PaletteColours[15])
				)
			{
				Blank = true;
				BlankColour = PaletteColours[15];
			}
//			else
			{
				Blank = false;

				if(Overlay)
				{
					if(DisplayMultiplexed)
					{
						int c = 256;
						while(c--)
						{
							Uint8 *DTB = &MultiDisplayTables[c << TableShift];

							DTB[YOffs1] = 
							DTB[YOffs2] = PaletteYUV[c][0];

							DTB[UOffs] = PaletteYUV[c][1];
							DTB[VOffs] = PaletteYUV[c][2];
						}
					}
					else
					{
						int c = 256;
						while(c--)
						{
							Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];

							int ic = 2, mc = c;
							while(ic--)
							{
								Uint32 Colour = PaletteColours[((mc&0x80) >> 4) | ((mc&0x20) >> 3) | ((mc&0x08) >> 2) | ((mc&0x02) >> 1)];

								DTB[YOffs1] = DTB[YOffs1+4] = DTB[YOffs1+16] = DTB[YOffs1+20] = 
								DTB[YOffs2] = DTB[YOffs2+4] = DTB[YOffs2+16] = DTB[YOffs2+20] = (Uint8)(Colour >> 16);

								DTB[VOffs] = DTB[VOffs+4] = DTB[VOffs+16] = DTB[VOffs+20] = (Uint8)((Colour >> 8)&0xff);
								DTB[UOffs] = DTB[UOffs+4] = DTB[UOffs+16] = DTB[UOffs+20] = (Uint8)(Colour&0xff);

								DTB += 8;
								mc <<= 1;
							}
						}
					}
				}
				else
				{
					int c = 256;

					switch(FrameBuffer->format->BytesPerPixel | (DisplayMultiplexed ? 8 : 0))
					{
						case 9:
							while(c--)
							{
								Uint8 *DTB = &MultiDisplayTables[c<<TableShift];
								DTB[0] = DTB[1] = (Uint8)PaletteColours[c];
							}
						break;

						case 1:
							while(c--)
							{
								Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] =
								DTB[2] = 
								DTB[3] =
								DTB[8] = 
								DTB[9] =
								DTB[10] = 
								DTB[11] = (Uint8)PaletteColours[((c&0x80) >> 4) | ((c&0x20) >> 3) | ((c&0x08) >> 2) | ((c&0x02) >> 1)];

								DTB[4] = 
								DTB[5] =
								DTB[6] = 
								DTB[7] =
								DTB[12] =
								DTB[13] =
								DTB[14] = 
								DTB[15] = (Uint8)PaletteColours[((c&0x40) >> 3) | ((c&0x10) >> 2) | ((c&0x04) >> 1) | (c&0x01)];
							}
						break;

						case 10:
							while(c--)
							{
								Uint16 *DTB = (Uint16 *)(&MultiDisplayTables[c << TableShift]);
								DTB[0] = DTB[1] = (Uint16)PaletteRGB[c];
							}
						break;

						case 2:
							while(c--)
							{
								Uint16 *DTB = (Uint16 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] =
								DTB[2] = 
								DTB[3] =
								DTB[8] = 
								DTB[9] =
								DTB[10] = 
								DTB[11] = (Uint16)PaletteColours[((c&0x80) >> 4) | ((c&0x20) >> 3) | ((c&0x08) >> 2) | ((c&0x02) >> 1)];

								DTB[4] = 
								DTB[5] =
								DTB[6] = 
								DTB[7] =
								DTB[12] =
								DTB[13] =
								DTB[14] = 
								DTB[15] = (Uint16)PaletteColours[((c&0x40) >> 3) | ((c&0x10) >> 2) | ((c&0x04) >> 1) | (c&0x01)];
							}
						break;

						case 11:
							while(c--)
							{
								Uint8 *DTB = &MultiDisplayTables[c << TableShift];

								DTB[0] = DTB[3] = ((Uint8 *)&PaletteRGB[c]) [0];
								DTB[1] = DTB[4] = ((Uint8 *)&PaletteRGB[c]) [1];
								DTB[2] = DTB[5] = ((Uint8 *)&PaletteRGB[c]) [2];
							}
						break;

						case 3:
							while(c--)
							{
								Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];
								Uint32 Colour;
								memset(DTB, 0, 64);

								Colour =  PaletteColours[((c&0x80) >> 4) | ((c&0x20) >> 3) | ((c&0x08) >> 2) | ((c&0x02) >> 1)];
								*((Uint32 *)&DTB[0]) |= Colour;
								*((Uint32 *)&DTB[3]) |= Colour;
								*((Uint32 *)&DTB[6]) |= Colour;
								*((Uint32 *)&DTB[9]) |= Colour;
								*((Uint32 *)&DTB[32]) |= Colour;
								*((Uint32 *)&DTB[35]) |= Colour;
								*((Uint32 *)&DTB[38]) |= Colour;
								*((Uint32 *)&DTB[41]) |= Colour;

								Colour = PaletteColours[((c&0x40) >> 3) | ((c&0x10) >> 2) | ((c&0x04) >> 1) | (c&0x01)];
								*((Uint32 *)&DTB[12]) |= Colour;
								*((Uint32 *)&DTB[15]) |= Colour;
								*((Uint32 *)&DTB[18]) |= Colour;
								*((Uint32 *)&DTB[21]) |= Colour;
								*((Uint32 *)&DTB[44]) |= Colour;
								*((Uint32 *)&DTB[47]) |= Colour;
								*((Uint32 *)&DTB[50]) |= Colour;
								*((Uint32 *)&DTB[53]) |= Colour;
							}
						break;

						case 12:
							while(c--)
							{
								Uint32 *DTB = (Uint32 *)&MultiDisplayTables[c << TableShift];
								DTB[0] = DTB[1] = PaletteRGB[c];
							}
						break;

						case 4:
							while(c--)
							{
								Uint32 *DTB = (Uint32 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] =
								DTB[2] = 
								DTB[3] =
								DTB[8] = 
								DTB[9] =
								DTB[10] = 
								DTB[11] = PaletteColours[((c&0x80) >> 4) | ((c&0x20) >> 3) | ((c&0x08) >> 2) | ((c&0x02) >> 1)];

								DTB[4] = 
								DTB[5] =
								DTB[6] = 
								DTB[7] =
								DTB[12] =
								DTB[13] =
								DTB[14] = 
								DTB[15] = PaletteColours[((c&0x40) >> 3) | ((c&0x10) >> 2) | ((c&0x04) >> 1) | (c&0x01)];
							}
						break;
					}
				}
			}
		break;

		case 4:
		case 6:
			if(PaletteColours[8] == PaletteColours[0])
			{
				Blank = true;
				BlankColour = PaletteColours[0];
			}
//			else
			{
				Blank = false;

				if(Overlay)
				{
					if(DisplayMultiplexed)
					{
						int c = 256;
						while(c--)
						{
							Uint8 *DTB = &MultiDisplayTables[c<<TableShift];

							DTB[YOffs1] = DTB[YOffs2] = PaletteYUV[c >> 4][0];
							DTB[UOffs] = PaletteYUV[c >> 4][1];
							DTB[VOffs] = PaletteYUV[c >> 4][2];

							DTB[YOffs1+4] = DTB[YOffs2+4] = PaletteYUV[c&0x0f][0];
							DTB[UOffs+4] = PaletteYUV[c&0x0f][1];
							DTB[VOffs+4] = PaletteYUV[c&0x0f][2];
						}
					}
					else
					{
						int c = 256;
						while(c--)
						{
							Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];

							int ic = 4, mc = c;
							while(ic--)
							{
								Uint32 Colour1 = (mc&0x80) ? PaletteColours[8] : PaletteColours[0];
								Uint32 Colour2 = (mc&0x40) ? PaletteColours[8] : PaletteColours[0];

								DTB[YOffs1] = DTB[YOffs2]		= (Uint8)(Colour1 >> 16);
								DTB[YOffs1+4] = DTB[YOffs2+4]	= (Uint8)(Colour2 >> 16);
								DTB[VOffs]		= (Uint8)((Colour1 >> 8)&0xff);
								DTB[VOffs+4]	= (Uint8)((Colour2 >> 8)&0xff);
								DTB[UOffs]		= (Uint8)(Colour1&0xff);
								DTB[UOffs+4]	= (Uint8)(Colour2&0xff);

								DTB += 8;
								mc <<= 2;
							}
						}
					}
				}
				else
				{
					int c = 256;

					switch(FrameBuffer->format->BytesPerPixel | (DisplayMultiplexed ? 8 : 0))
					{
						case 9:
							while(c--)
							{
								Uint8 *DTB = &MultiDisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] = (Uint8)PaletteColours[c >> 4];
								DTB[2] = 
								DTB[3] = (Uint8)PaletteColours[c&0x0f];
							}
						break;

						case 1:
							while(c--)
							{
								Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] = (c&0x80) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];
								DTB[2] = 
								DTB[3] = (c&0x40) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];
								DTB[4] = 
								DTB[5] = (c&0x20) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];
								DTB[6] = 
								DTB[7] = (c&0x10) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];
								DTB[8] = 
								DTB[9] = (c&0x08) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];
								DTB[10] = 
								DTB[11] = (c&0x04) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];
								DTB[12] = 
								DTB[13] = (c&0x02) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];
								DTB[14] = 
								DTB[15] = (c&0x01) ? (Uint8)PaletteColours[8] : (Uint8)PaletteColours[0];
							}
						break;

						case 10:
							while(c--)
							{
								Uint16 *DTB = (Uint16 *)(&MultiDisplayTables[c << TableShift]);
								DTB[0] = DTB[1] = (Uint16)PaletteRGB[c >> 4];
								DTB[2] = DTB[3] = (Uint16)PaletteRGB[c&0x0f];
							}
						break;

						case 2:
							while(c--)
							{
								Uint16 *DTB = (Uint16 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] = (c&0x80) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[2] = 
								DTB[3] = (c&0x40) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[4] = 
								DTB[5] = (c&0x20) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[6] = 
								DTB[7] = (c&0x10) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[8] = 
								DTB[9] = (c&0x08) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[10] = 
								DTB[11] = (c&0x04) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[12] = 
								DTB[13] = (c&0x02) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
								DTB[14] = 
								DTB[15] = (c&0x01) ? (Uint16)PaletteColours[8] : (Uint16)PaletteColours[0];
							}
						break;

						case 11:
							while(c--)
							{
								Uint8 *DTB = &MultiDisplayTables[c << TableShift];

								DTB[0] = DTB[3] = ((Uint8 *)&PaletteRGB[c >> 4]) [0];
								DTB[1] = DTB[4] = ((Uint8 *)&PaletteRGB[c >> 4]) [1];
								DTB[2] = DTB[5] = ((Uint8 *)&PaletteRGB[c >> 4]) [2];

								DTB[6] = DTB[9] = ((Uint8 *)&PaletteRGB[c&0x0f]) [0];
								DTB[7] = DTB[10] = ((Uint8 *)&PaletteRGB[c&0x0f]) [1];
								DTB[8] = DTB[11] = ((Uint8 *)&PaletteRGB[c&0x0f]) [2];
							}
						break;

						case 3:
							while(c--)
							{
								Uint8 *DTB = (Uint8 *)&DisplayTables[c<<TableShift];
								memset(DTB, 0, 64);

								*((Uint32 *)&DTB[0]) |=		(c&0x80) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[3]) |=		(c&0x80) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[6]) |=		(c&0x40) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[9]) |=		(c&0x40) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[12]) |=	(c&0x20) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[15]) |=	(c&0x20) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[18]) |=	(c&0x10) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[21]) |=	(c&0x10) ? PaletteColours[8] : PaletteColours[0];

								*((Uint32 *)&DTB[32]) |=	(c&0x08) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[35]) |=	(c&0x08) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[38]) |=	(c&0x04) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[41]) |=	(c&0x04) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[44]) |=	(c&0x02) ? PaletteColours[8] : PaletteColours[0]; 
								*((Uint32 *)&DTB[47]) |=	(c&0x02) ? PaletteColours[8] : PaletteColours[0];
								*((Uint32 *)&DTB[50]) |=	(c&0x01) ? PaletteColours[8] : PaletteColours[0]; 
								*((Uint32 *)&DTB[53]) |=	(c&0x01) ? PaletteColours[8] : PaletteColours[0];
							}
						break;

						case 12:
							while(c--)
							{
								Uint32 *DTB = (Uint32 *)&MultiDisplayTables[c << TableShift];
								DTB[0] = DTB[1] = PaletteRGB[c >> 4];
								DTB[2] = DTB[3] = PaletteRGB[c&0x0f];
							}
						break;

						case 4:
							while(c--)
							{
								Uint32 *DTB = (Uint32 *)&DisplayTables[c<<TableShift];

								DTB[0] = 
								DTB[1] = (c&0x80) ? PaletteColours[8] : PaletteColours[0];
								DTB[2] = 
								DTB[3] = (c&0x40) ? PaletteColours[8] : PaletteColours[0];
								DTB[4] = 
								DTB[5] = (c&0x20) ? PaletteColours[8] : PaletteColours[0];
								DTB[6] = 
								DTB[7] = (c&0x10) ? PaletteColours[8] : PaletteColours[0];
								DTB[8] = 
								DTB[9] = (c&0x08) ? PaletteColours[8] : PaletteColours[0];
								DTB[10] = 
								DTB[11] = (c&0x04) ? PaletteColours[8] : PaletteColours[0];
								DTB[12] = 
								DTB[13] = (c&0x02) ? PaletteColours[8] : PaletteColours[0];
								DTB[14] = 
								DTB[15] = (c&0x01) ? PaletteColours[8] : PaletteColours[0];
							}
						break;
					}
				}
			}
		break;
	}

	if(Overlay)
	{
		Uint8 Y, U, V;
		Y = (Uint8)(BlankColour >> 16);
		V = (Uint8)(BlankColour >> 8);
		U = (Uint8)BlankColour;
		BlankColour = (Y << (YOffs1*8)) + (Y << (YOffs2*8)) + (U << (UOffs*8)) + (V << (VOffs*8));
	}
}

#define MapRGB(r, g, b) Overlay ? YUVTable[r|g|b] : SDL_MapRGB( FrameBuffer->format, r?0:0xff, g?0:0xff, b?0:0xff);

void CDisplay::RecalculatePalette()
{
	PaletteColours[0] =	MapRGB(
						((PaletteBytes[0x01]&0x01) << 2),
						((PaletteBytes[0x01]&0x10) >> 3),
						((PaletteBytes[0x00]&0x10) >> 4)
						);

	PaletteColours[2] =	MapRGB(
						((PaletteBytes[0x01]&0x02) << 1),
						((PaletteBytes[0x01]&0x20) >> 4),
						((PaletteBytes[0x00]&0x20) >> 5)
						);

	PaletteColours[8] =	MapRGB(
						((PaletteBytes[0x01]&0x04) << 0),
						((PaletteBytes[0x00]&0x04) >> 1),
						((PaletteBytes[0x00]&0x40) >> 6)
						);

	PaletteColours[10] = MapRGB(
						((PaletteBytes[0x01]&0x08) >> 1),
						((PaletteBytes[0x00]&0x08) >> 2),
						((PaletteBytes[0x00]&0x80) >> 7)
						);

	PaletteColours[4] =	MapRGB(
						((PaletteBytes[0x03]&0x01) << 2),
						((PaletteBytes[0x03]&0x10) >> 3),
						((PaletteBytes[0x02]&0x10) >> 4)
						);

	PaletteColours[6] =	MapRGB(
						((PaletteBytes[0x03]&0x02) << 1),
						((PaletteBytes[0x03]&0x20) >> 4),
						((PaletteBytes[0x02]&0x20) >> 5)
						);

	PaletteColours[12] = MapRGB(
						((PaletteBytes[0x03]&0x04) << 0),
						((PaletteBytes[0x02]&0x04) >> 1),
						((PaletteBytes[0x02]&0x40) >> 6)
						);

	PaletteColours[14] = MapRGB(
						((PaletteBytes[0x03]&0x08) >> 1),
						((PaletteBytes[0x02]&0x08) >> 2),
						((PaletteBytes[0x02]&0x80) >> 7)
						);

	PaletteColours[5] =	MapRGB(
						((PaletteBytes[0x05]&0x01) << 2),
						((PaletteBytes[0x05]&0x10) >> 3),
						((PaletteBytes[0x04]&0x10) >> 4)
						);

	PaletteColours[7] =	MapRGB(
						((PaletteBytes[0x05]&0x02) << 1),
						((PaletteBytes[0x05]&0x20) >> 4),
						((PaletteBytes[0x04]&0x20) >> 5)
						);

	PaletteColours[13] = MapRGB(
						((PaletteBytes[0x05]&0x04) << 0),
						((PaletteBytes[0x04]&0x04) >> 1),
						((PaletteBytes[0x04]&0x40) >> 6)
						);

	PaletteColours[15] = MapRGB(
						((PaletteBytes[0x05]&0x08) >> 1),
						((PaletteBytes[0x04]&0x08) >> 2),
						((PaletteBytes[0x04]&0x80) >> 7)
						);

	PaletteColours[1] =	MapRGB(
						((PaletteBytes[0x07]&0x01) << 2),
						((PaletteBytes[0x07]&0x10) >> 3),
						((PaletteBytes[0x06]&0x10) >> 4)
						);

	PaletteColours[3] =	MapRGB(
						((PaletteBytes[0x07]&0x02) << 1),
						((PaletteBytes[0x07]&0x20) >> 4),
						((PaletteBytes[0x06]&0x20) >> 5)
						);

	PaletteColours[9] =	MapRGB(
						((PaletteBytes[0x07]&0x04) << 0),
						((PaletteBytes[0x06]&0x04) >> 1),
						((PaletteBytes[0x06]&0x40) >> 6)
						);

	PaletteColours[11] = MapRGB(
						((PaletteBytes[0x07]&0x08) >> 1),
						((PaletteBytes[0x06]&0x08) >> 2),
						((PaletteBytes[0x06]&0x80) >> 7)
						);
}

void CDisplay::SetMultiplexedPalette(SDL_Color *Pal)
{
	/* copy to SrcPaletteRGB, convert to PaletteYUV and current RGB mode */
	int c = 256;
	while(c--)
	{
		SrcPaletteRGB[c].r = Pal[c].r;
		SrcPaletteRGB[c].g = Pal[c].g;
		SrcPaletteRGB[c].b = Pal[c].b;

		PaletteRGB[c] = SDL_MapRGB( FrameBuffer->format, SrcPaletteRGB[c].r, SrcPaletteRGB[c].g, SrcPaletteRGB[c].b);

		/* and convert to YUV colours */
		float R = (float)SrcPaletteRGB[c].r / 255.0f;
		float G = (float)SrcPaletteRGB[c].g / 255.0f;
		float B = (float)SrcPaletteRGB[c].b / 255.0f;

		float Y = 16	+ R*65.481f	+ G*128.553f	+ B*24.966f;
		float U = 128	- R*37.797f - G*74.203f		+ B*112.0f;
		float V = 128	+ R*112.0f	- G*93.786f		- B*18.214f;

		if(Y < 0) Y = 0;	if(Y > 255) Y = 255;
		if(U < 0) U = 0;	if(U > 255) U = 255;
		if(V < 0) V = 0;	if(V > 255) V = 255;

		PaletteYUV[c][0] = (int)Y;
		PaletteYUV[c][1] = (int)U;
		PaletteYUV[c][2] = (int)V;
	}

	/* if currently working in 8bpp, set palette */
	if(!Overlay && FrameBuffer->format->BytesPerPixel == 1)
		SDL_SetColors(FrameBuffer, SrcPaletteRGB, 0, 256);

	RefillGraphicsTables();
}
