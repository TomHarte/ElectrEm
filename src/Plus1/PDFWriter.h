#ifndef __PDFWRITER_H
#define __PDFWRITER_H

class CPDF
{
	private:
		FILE *PDFFile;

		/* all objects are put in here */
		struct Page;
		int NumObjects;
		struct Obj
		{
			unsigned int id;
			long FileOffset;
			Obj *Next;
		} *ObjList, **ObjTail;

		struct Page
		{
			Obj **FirstObj;
			int PageNum;
			long FileOffset;
			unsigned int id;
			int Width, Height;
			Page *Next;
		} *PageList, **PageTail;
		int NumPages;

		struct Font
		{
			char Name[128];
			unsigned int id;

			FX80Glyph Glyphs[256];
			FX80State State;

			Font *Next;
		} *FontList;

		Obj *NewObject();
		long StreamLengthPos;
		long StreamLengthBegin;
		int CurPageHeight;

	public:
		int GetPageHeight() {return CurPageHeight;}
		void NewPage(int Width, int Height);
		void AddFont(char *Name, FX80Glyph *GlyphTable, FX80State &State);

		void Open(char *fname);
		void Close();

		void NewStream();
		void AddText(char *);
		void AddPoint(float x, float y);
		void EndStream();
};

#endif
