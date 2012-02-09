#ifndef __PLUS1_H
#define __PLUS1_H

#include <stdio.h>

#include "../ComponentBase.h"
#include "../ProcessPool.h"

#define PLUS1_PRINTBUFFERSIZE	4096

#define	FX80MODE_USA			0
#define	FX80MODE_FRANCE			1
#define	FX80MODE_GERMANY		2
#define	FX80MODE_UK				3
#define	FX80MODE_DENMARK		4
#define	FX80MODE_SWEDEN			5
#define	FX80MODE_ITALY			6
#define	FX80MODE_SPAIN			7
#define	FX80MODE_JAPAN			8
#define	FX80MODE_NORWAY			9
#define	FX80MODE_DENMARKII		10

#define FX80PAPERSIZE_A4		0
#define FX80PAPERSIZE_USLEGAL	1
#define FX80PAPERSIZE_USLETTER	2

/*
	VIRTUAL_DPI needs to be a multiple of:

			240				= 2 * 2 * 2 * 2 * 3 * 5
			(60, 120, 80)
			90				= 2 * 3 * 3 * 5
			216				= 2 * 2 * 2 * 3 * 3 * 3
			(72)
			206				= 2 * 103
			144				= 2 * 2 * 2 * 2 * 3 * 3

	Output DPI is (currently) 720 dpi, so 720 needs to be a divider too
			720				= 2 * 2 * 2 * 2 * 3 * 3 * 5

	= 2 * 2 * 2 * 2 * 3 * 3 * 3 * 5 * 103 = 222480
*/

#define VIRTUAL_DPI			(222480)
#define OUTPUT_DPI			720
#define DPI_TO_PIXELS(x)	((x) / (VIRTUAL_DPI / OUTPUT_DPI) )
#define DPI_TO_POINTS(x)	((x) / (VIRTUAL_DPI / 72) )

#define GLYPHLINE_START		(1 << 18)

enum FX80HeightMode {HM_NORMAL, HM_SUPERSCRIPT, HM_SUBSCRIPT};

struct FX80State
{
	bool DoubleStrike;
	bool Emphasis;
	bool Underline;
	bool ExpandedMode;
	bool EliteMode;
	bool CondensedMode;
	bool Proportional;
	FX80HeightMode HeightMode;
};

class FX80Glyph
{
	private:
		int StartColumn, EndColumn;
		Uint32 Image[11];

		/* these look like they duplicate members of CPlus1, but this function applies the correct priorities */
		static bool DoubleStrike;
		static bool Underline;
		static bool ExpandedMode;
		static bool Proportional;
		static int ColumnAdvance;
		static FX80HeightMode HeightMode;
		static int XAdvance;
		static bool Emphasis, EmphasisToggle;
		static int CurLine;

	public:
		void SetFromMask(int *Mask, int Start, int End);

		int Begin(FX80State &);
		void Begin();
		Uint32 *GetLine();
		int GetXAdvance();
		int GetWidth();
};

#include "PDFWriter.h"

class CPlus1: public CComponentBase
{
	public:
		CPlus1();
		virtual ~CPlus1();

		void AttachTo(CProcessPool &pool, Uint32 id);
		bool Write(Uint16 Addr, Uint32 TimeStamp, Uint8 Data8, Uint32 Data32);
		bool Read(Uint16 Addr, Uint32 TimeStamp, Uint8 &Data8, Uint32 &Data32);
		Uint32 Update(Uint32 TotalCycles, bool Catchup);

		/* functions for pointing the Plus 1 towards a target for printer output */
		bool SetPrinterTarget(char *fname);
		void CloseFile();
		void DoFormFeed();
		
		/* for collecting printer configuration */
		bool IOCtl(Uint32 Control, void *Parameter = NULL, Uint32 TimeStamp = 0);

	private:
		/* dumb function for adding a character to the printer output, called by Write */
		void BufferChar(char c);

		/* printer internal stuff */
		void FlushPrintBuffer();
		unsigned char PrintBuffer[PLUS1_PRINTBUFFERSIZE];
		unsigned int PrinterBufferPointer;

		/* one of these will be where data is put */
		FILE *PrinterTarget;
		Uint8 *PageData;
		CPDF *PDFTarget;

		char *FileName;
		SDL_mutex *PrinterFileMutex;

		/* ADC internal stuff */
		Uint32 ADCCyclesLeft;
		Uint8 ADCValue;
		Uint8 GetADCChannel(int channel);
		Uint8 GetADCButtons();

		int GraphicsWidth;
		enum {OM_PDF, OM_BITMAP, OM_RTF, OM_TXT, OM_RAW, OM_NONE} OutputMode;

		bool InText;
		void EnterText();
		void ExitText();

		int PageNum;
		
		/* output bits */
		void PrintChar(Uint8);

		FX80State State;
		void SetDoubleStrike(bool);
		void SetEmphasis(bool);
		void SetItalics(bool);			bool Italics;
		void SetUnderline(bool);
		void SetExpandedMode(bool);
		void SetEliteMode(bool);
		void SetCondensedMode(bool);
		void SetProportional(bool);

		void FixTextWidth();
		void FormFeed(bool Reverse = false);
		void LineFeed(bool Reverse = false);
		void CarriageReturn();
		void SetHeightMode(FX80HeightMode m);
		void Reset();

		bool SkipDot, SkipDotToggle;
		Uint8 ColourMask;

		int VerticalTabStops[16], HorizontalTabStops[32];

		void SetupFont();
		FX80Glyph RomFont[256], RamFont[256];
		bool UserFont;

		/* defaults / settings */
		int DefaultNationality;
		bool AutoLineFeed;
		bool Colour;

		/* */
		Uint8 PrintCharMapping[256];
		Uint8 PrintCharXor;
		void SetNationality(int Nationality);

		/* page info */
		int BitmapW, BitmapH;
		int PageW, PageH;
		int LeftMargin, RightMargin, BottomMargin;

		/* PNG stuff */
		void AllocateImage();
		void FreeImage();
		void SaveAndClearImage();
		void MovePrintHeadRelative(int x, int y);	// takes arguments in 720s of an inch (inches, yuck!)
		void MovePrintHeadAbsolute(int x, int y);	// takes arguments in 720s of an inch (inches, yuck!)
		bool CheckGlyphWidth(int w);
		void DoImpact(Uint32 Pattern, bool AdvanceSlow = true);
		void PutDot(int x, int y);
		void DoGlyph(FX80Glyph *);
//		void BackupHeadPosition();
//		void RestoreHeadPosition();
		int HeadX, HeadY, AddHeadX, LineSpacing;// BackupHeadX, BackupHeadY;
};

#endif
