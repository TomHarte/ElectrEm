#include "Plus1.h"
#include "zlib.h"

CPDF::Obj *CPDF::NewObject()
{
	Obj *R = *ObjTail = new Obj;
	ObjTail = &(*ObjTail)->Next;
	R->FileOffset = ftell(PDFFile);
	R->id = NumObjects+1;
	R->Next = NULL;
	NumObjects++;
	return R;
}

void CPDF::NewPage(int Width, int Height)
{
	NumPages++;
	Page *Pg = *PageTail = new Page;
	Pg->Next = NULL;
	Pg->PageNum = NumPages;
	Pg->FirstObj = ObjTail;
	Pg->Width = Width;
	CurPageHeight = Pg->Height = Height;
	PageTail = &(*PageTail)->Next;
}

void CPDF::AddFont(char *name, FX80Glyph *Data, FX80State &State)
{
	// check if this font is already included
	Font *F = FontList;
	while(F)
	{
		if(!strcmp(F->Name, name)) return;
		F = F->Next;
	}

	// add font
	F = new Font;
	F->Next = FontList;
	FontList = F;
	strcpy(F->Name, name);

	int c = 256;
	while(c--)
		F->Glyphs[c] = Data[c];
	F->State = State;
}

void CPDF::Open(char *fname)
{
	PDFFile = fopen(fname, "wb");
	fputs("%PDF-1.4\n", PDFFile);
	fprintf(PDFFile, "%%%c%c%c%c\n", 128, 255, 179, 163);
	NumObjects = NumPages = 0;

	PageList = NULL;
	PageTail = &PageList;

	ObjList = NULL;
	ObjTail = &ObjList;

	FontList = NULL;
}

void CPDF::Close()
{
	Page *P;
	Font *F;

	// write Courier font
	Obj *CourierObj = NewObject();
	fprintf(PDFFile, "%d 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>\nendobj\n", CourierObj->id);

	// write single dot XObject
	Obj *Dot = NewObject();
	fprintf(PDFFile, "%d 0 obj\n<< /Type /XObject /Subtype /Form /FormType 1 /BBox [-0.5 -0.5 0.5 0.5] /Marix [1 0 0 1 0 0] /Resources << /ProcSet [/PDF] >> /Length 148 >>\nstream\n", Dot->id);
	fprintf(PDFFile, "0 0.5 m 0.27553 0.5 0.5 0.27553 0.5 0 c 0.5 -0.27553 0.27553 -0.5 0 -0.5 c -0.27553 -0.5 -0.5 -0.27553 -0.5 0 c -0.5 0.27553 -0.27553 0.5 0 0.5 c f\n");
	fprintf(PDFFile, "endstream\nendobj\n");

	// write custom fonts
	F = FontList;
	while(F)
	{
		int c;
		Obj *FontHead = NewObject();
		F->id = FontHead->id;
		F->Glyphs[32].Begin(F->State);

		// 6/72 = 0.083333333333
		fprintf(PDFFile, "%d 0 obj\n<< /Type /Font /Subtype /Type3 /FontBBox [0 0 %0.2f 10] /FontMatrix [0.083333333333 0 0 0.083333333333 0 0]", FontHead->id, (float)F->Glyphs[32].GetWidth() / (VIRTUAL_DPI / 72));
		fprintf(PDFFile, " /CharProcs %d 0 R /Encoding %d 0 R /FirstChar 0 /LastChar 255 /Widths [", FontHead->id+2, FontHead->id+1);
		for(c = 0; c < 256; c++)
		{
			fprintf(PDFFile, "%0.2f ", (float)F->Glyphs[c].GetWidth() / (VIRTUAL_DPI / 72));
		} 
		fprintf(PDFFile, "] /ToUnicode %d 0 R >>\nendobj\n", FontHead->id+3);

		/* encoding */
		Obj *T = NewObject();
		fprintf(PDFFile, "%d 0 obj\n<< /Type /Encoding /BaseEncoding /WinAnsiEncoding /Differences [0 ", FontHead->id+1);
		for(c = 0; c < 256; c++)
		{
			fprintf(PDFFile, "/%s_%d ", F->Name, c);
		}
		fprintf(PDFFile, "] >>\nendobj\n");

		/* CharProcs */
		T = NewObject();
		fprintf(PDFFile, "%d 0 obj\n<< ", FontHead->id+2);
		for(c = 0; c < 256; c++)
		{
			fprintf(PDFFile, "/%s_%d %d 0 R ", F->Name, c, FontHead->id+4+c);
		}
		fprintf(PDFFile, ">>\nendobj\n");

		/* ToUnicode (always the same) */
		NewStream();
			AddText("/CIDInit /ProcSet findresource begin\n");
			AddText("12 dict begin\n");
			AddText("begincmap\n");
			AddText("/CIDSystemInfo\n");
			AddText("<< /Registry (Adobe)\n");
			AddText("/Ordering (UCS)\n");
			AddText("/Supplement 0\n");
			AddText(">> def\n");
			AddText("/CMapName /Adobe-Identity-UCS def\n");
			AddText("/CMapNameType 2 def\n");
			AddText("1 begincodespacerange\n");
			AddText("<00> <FF>\n");
			AddText("endcodespacerange\n");
			AddText("2 beginbfrange\n");

			for(int base = 0; base < 0x100; base += 0x80)
			{
				Uint16 First32Targets[32] =
				{
					0x00e0, 0x00e8, 0x00f9, 0x00f2,	// à, è, ù, ò
					0x00ec, 0x00ba, 0x00a3, 0x00a1,	// ì, º, £, ¡
					0x00bf, 0x00d1, 0x00f1, 0x00a4,	// ¿, Ñ, ñ, ¤
					0xfb05, 0x00c5, 0x00e5, 0x00e7,	// ﬅ, Å, å, ç	- ligature might be wrong
					0x00a7, 0x00df, 0x00c6, 0x00e6,	// §, ß, Æ, æ
					0x00d8, 0x00f8, 0x00a8, 0x00c4,	// Ø, ø, ¨, Ä
					0x00d6, 0x00dc, 0x00e4, 0x00f6,	// Ö, Ü, ä, ö
					0x00df, 0x00c9, 0x00e9, 0xffe5	// ü, É, é, ￥
				};
				char Buffer[32];

				/* table based mapping, for Latin supplement characters + the occasional oddity */
				sprintf(Buffer, "<%02x> <%02x> [ ", base, base+0x1f);
				AddText(Buffer);
				for(int c = 0; c < 32; c++)
				{
					sprintf(Buffer, "<%04x> ", First32Targets[c]);
					AddText(Buffer);
				}
				AddText("]\n");

				/* ASCII area */
				sprintf(Buffer, "<%02x> <%02x> <0020>\n", base+0x20, base+0x7e);
				AddText(Buffer);

				/* zero with a slash (should have a combining long solidus overlay, can't seem to make that work) */
				sprintf(Buffer, "<%02x> <%02x> <0030>\n", base+0x7f, base+0x7f);
				AddText(Buffer);
			}

			AddText("endbfrange\n");
			AddText("endcmap\n");
			AddText("CMapName currentdict /CMap defineresource pop\n");
			AddText("end\n");
			AddText("end\n");
		EndStream();

		/* actual glyphs */
		for(c = 0; c < 256; c++)
		{
			Uint32 *M;
			F->Glyphs[c].Begin();
			float x = 0;
			NewStream();

			char TBuf[128];
			sprintf(TBuf, "%0.2f 9 0 0 %0.2f 9 d1 ", (float)F->Glyphs[c].GetWidth() / (VIRTUAL_DPI / 72), (float)F->Glyphs[c].GetWidth() / (VIRTUAL_DPI / 72));
			AddText(TBuf);
			while(M = F->Glyphs[c].GetLine())
			{
				float y = 9;
				Uint32 RC = *M;
				while(RC&((GLYPHLINE_START << 1)-1))
				{
					if(RC&GLYPHLINE_START)
						AddPoint( x, y);
					y -= 0.5f;
					RC <<= 1;
				}
				x += (float)F->Glyphs[c].GetXAdvance() / (VIRTUAL_DPI / 72);
			}
			EndStream();
		}

		F = F->Next;
	}
	
/*
	while(mask&((GLYPHLINE_START << 1)-1))
	{
		if(mask&GLYPHLINE_START) PutDot(PosX, PosY);
		PosY += 5;
		mask <<= 1;
	}
*/

/*	Obj *FontObj = NewObject();
	fprintf(PDFFile, "%d 0 obj\n<< /Type /Font /Subtype /Type3 /FontBBox [0 0 750 750] /FontMatrix [0.001 0 0 0.001 0 0]", FontObj->id);
	fprintf(PDFFile, " /CharProcs %d 0 R /Encoding %d 0 R /FirstChar 97 /LastChar 98 /Widths [1000 1000] >>\nendobj\n", FontObj->id+2, FontObj->id+1);

	Obj *T = NewObject();
	fprintf(PDFFile, "%d 0 obj\n<< /Type /Encoding /Differences [97 /square /triangle] >>\nendobj\n", FontObj->id+1);

	T = NewObject();
	fprintf(PDFFile, "%d 0 obj\n<< /square %d 0 R /triangle %d 0 R >>\nendobj\n", FontObj->id+2, FontObj->id+3, FontObj->id+4);

	NewStream();
		AddText("1000 0 0 0 750 750 d1 0 0 750 750 re f ");
	EndStream();
	NewStream();
		AddText("1000 0 0 0 750 750 d1 0 0 m 375 750 l 750 0 l f ");
	EndStream();*/

//	fprintf(PDFFile, "%d 0 obj\n<< /square %d 0 R /triangle %d 0 R >>\nendobj\n", FontObj->id+2, FontObj->id+3, FontObj->id+4);

	// write actual pages
	P = PageList;
	int ObjectsIn = NumObjects;
	while(P)
	{
		P->FileOffset = ftell(PDFFile);
		P->id = NumObjects+1; NumObjects++;
		fprintf(PDFFile, "%d 0 obj\n\t", P->id);		// page entry (just points to the outlines)
		fputs("<< /Type /Page\n\t\t", PDFFile);
		fprintf(PDFFile, "/Parent %d 0 R\n\t\t", ObjectsIn+NumPages+1);
		fprintf(PDFFile, "/MediaBox [0 0 %d %d]\n\t\t", P->Width, P->Height);
		fputs("/Contents ", PDFFile);
		Obj *O = *(P->FirstObj);

		if(P->Next)
		{
			while(O != *P->Next->FirstObj)
			{
				fprintf(PDFFile, "%d 0 R ", O->id);
				O = O->Next;
			}
		}
		else
		{
			while(O != CourierObj)
			{
				fprintf(PDFFile, "%d 0 R ", O->id);
				O = O->Next;
			}
		}
		fputs(">>\nendobj\n", PDFFile);

		P = P->Next;
	}

	// write pages entry
	long PagesPos = ftell(PDFFile);
	fprintf(PDFFile, "%d 0 obj\n\t", NumObjects+1); NumObjects++;		// pages entry (there is one)
	fputs("<< /Type /Pages\n\t\t", PDFFile);
	fprintf(PDFFile, "/Resources << /XObject << /Dot %d 0 R >> /Font << /Courier %d 0 R ", Dot->id, CourierObj->id);
	F = FontList;
	while(F)
	{
		fprintf(PDFFile, "/%s %d 0 R ", F->Name, F->id);
		F = F->Next;
	}
	fprintf(PDFFile, " >> >>\n\t\t");
	fputs("/Kids [", PDFFile);
	P = PageList;
	while(P)
	{
		fprintf(PDFFile, "%d 0 R ", P->id);
		P = P->Next;
	}
	fputs("]\n\t\t", PDFFile);
	fprintf(PDFFile, "/Count %d\n\t", NumPages);
	fputs(">>\nendobj\n", PDFFile);

	// write catalogue
	long CataloguePos = ftell(PDFFile);
	fprintf(PDFFile, "%d 0 obj\n\t", NumObjects+1); NumObjects++;
	fprintf(PDFFile, "<< /Type /Catalog /Pages %d 0 R >>\nendobj\n", NumObjects-1);

	// write xref table
	long xrefpos = ftell(PDFFile);
	fprintf(PDFFile, "xref\n0 %d\n", NumObjects+1);
	fprintf(PDFFile, "0000000000 65535 f \n");
	Obj *O = ObjList;
	while(O)
	{
		fprintf(PDFFile, "%010d 00000 n \n", (int)O->FileOffset);
		O = O->Next;
	}
	P = PageList;
	while(P)
	{
		fprintf(PDFFile, "%010d 00000 n \n", (int)P->FileOffset);
		P = P->Next;
	}
	fprintf(PDFFile, "%010d 00000 n \n", (int)PagesPos);
	fprintf(PDFFile, "%010d 00000 n \n", (int)CataloguePos);

	fprintf(PDFFile, "trailer\n\t<</Size %d\n\t\t/Root %d 0 R\n\t>>\n", NumObjects+1, NumObjects);
	fprintf(PDFFile, "startxref\n%d\n", (int)xrefpos);
	fputs("%%EOF", PDFFile);

	fclose(PDFFile);

	// free memory
	while(PageList)
	{
		Page *N = PageList->Next;
		delete PageList;
		PageList = N;
	}
	while(ObjList)
	{
		Obj *N = ObjList->Next;
		delete ObjList;
		ObjList = N;
	}
	while(FontList)
	{
		Font *N = FontList->Next;
		delete FontList;
		FontList = N;
	}
}

z_stream z;

void CPDF::NewStream()
{
	deflateInit(&z, Z_BEST_COMPRESSION);

	Obj *O = NewObject();
	fprintf(PDFFile, "%d 0 obj\n\t", O->id);
	fprintf(PDFFile, "<< /Filter /FlateDecode /Length ");
	StreamLengthPos = ftell(PDFFile);
	fprintf(PDFFile, "            >>\nstream\n");
	StreamLengthBegin = ftell(PDFFile);
}

void CPDF::AddText(char *t)
{
	Uint8 OutputBuffer[1024];
	z.avail_out = 1024;
	z.next_out = OutputBuffer;
	z.avail_in = strlen(t);
	z.next_in = (Bytef *)t;
	deflate(&z, Z_NO_FLUSH);

	if(1024 - z.avail_out)
		fwrite(OutputBuffer, 1, 1024-z.avail_out, PDFFile);
	//printf("z out: %d\n", 1024 - z.avail_out);

//	fputs(t, PDFFile);
}

void CPDF::AddPoint(float x, float y)// in points
{
	char TBuf[2048];
	sprintf(TBuf, "q 1 0 0 1 %f %f cm ", x+0.5, y+0.5);
	AddText(TBuf);
	AddText("/Dot Do Q ");
//	AddText("0 0.5 m 0.27553 0.5 0.5 0.27553 0.5 0 c 0.5 -0.27553 0.27553 -0.5 0 -0.5 c -0.27553 -0.5 -0.5 -0.27553 -0.5 0 c -0.5 0.27553 -0.27553 0.5 0 0.5 c f Q ");
}

void CPDF::EndStream()
{
	Uint8 OutputBuffer[1024];
	while(1)
	{
		z.avail_out = 1024;
		z.next_out = OutputBuffer;
		int res = deflate(&z, Z_FINISH);
		if(1024 - z.avail_out)
			fwrite(OutputBuffer, 1, 1024-z.avail_out, PDFFile);
		
		if(res != Z_OK)
			break;
	}

	deflateEnd(&z);

	long EndPos = ftell(PDFFile);
	fseek(PDFFile, StreamLengthPos, SEEK_SET);
	fprintf(PDFFile, "%d", (int)(EndPos - StreamLengthBegin));
	fseek(PDFFile, 0, SEEK_END);
	fprintf(PDFFile, "\nendstream\nendobj\n");	
}
