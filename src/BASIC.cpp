/*

	ElectrEm (c) 2000-6 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	BASIC.cpp
	=========

	This is the code to take an Electron memory dump and then save
	the current BASIC file to an ASCII file, or alternatively to
	open an ASCII file, tokenise it and put it into memory

*/

#include "BASIC.h"
#include <stdio.h>

/*

	KeyWord table - used by both the exporter and the importer to convert
	between the plain text version of a BASIC program and the format used
	internally by the BASIC ROM

	Information taken from pages 41-43 of the BASIC ROM User Guide, 
	BASIC 2 ROM assumed throughout

*/

struct KeyWord
{
	char *Name;
	Uint8 Flags;
	unsigned int StrLen;
	struct KeyWord *Next;
};

struct KeyWord KeyWordTable[0x80] =
{
	/* 0x80 */
	{"AND",		0x00},		{"DIV",		0x00},		{"EOR",		0x00},		{"MOD",		0x00},
	{"OR",		0x00},		{"ERROR",	0x04},		{"LINE",	0x00},		{"OFF",		0x00},

	/* 0x88 */
	{"STEP",	0x00},		{"SPC",		0x00},		{"TAB(",	0x00},		{"ELSE",	0x14},
	{"THEN",	0x14},		{"",		0x00},		{"OPENIN",	0x00},		{"PTR",		0x43},

	/* 0x90 */
	{"PAGE",	0x43},		{"TIME",	0x43},		{"LOMEM",	0x43},		{"HIMEM",	0x43},
	{"ABS",		0x00},		{"ACS",		0x00},		{"ADVAL",	0x00},		{"ASC",		0x00},

	/* 0x98 */
	{"ASN",		0x00},		{"ATN",		0x00},		{"BGET",	0x01},		{"COS",		0x00},
	{"COUNT",	0x01},		{"DEG",		0x00},		{"ERL",		0x01},		{"ERR",		0x01},

	/* 0xa0 */
	{"EVAL",	0x00},		{"EXP",		0x00},		{"EXT",		0x01},		{"FALSE",	0x01},
	{"FN",		0x08},		{"GET",		0x00},		{"INKEY",	0x00},		{"INSTR(",	0x00},

	/* 0xa8 */
	{"INT",		0x00},		{"LEN",		0x00},		{"LN",		0x00},		{"LOG",		0x00},
	{"NOT",		0x00},		{"OPENUP",	0x00},		{"OPENOUT",	0x00},		{"PI",		0x01},

	/* 0xb0 */
	{"POINT(",	0x00},		{"POS",		0x01},		{"RAD",		0x00},		{"RND",		0x01},
	{"SGN",		0x00},		{"SIN",		0x00},		{"SQR",		0x00},		{"TAN",		0x00},

	/* 0xb8 */
	{"TO",		0x00},		{"TRUE",	0x01},		{"USR",		0x00},		{"VAL",		0x00},
	{"VPOS",	0x01},		{"CHR$",	0x00},		{"GET$",	0x00},		{"INKEY$",	0x00},

	/* 0xc0 */
	{"LEFT$(",	0x00},		{"MID$(",	0x00},		{"RIGHT$(",	0x00},		{"STR$",	0x00},
	{"STRING$(",0x00},		{"EOF",		0x01},		{"AUTO",	0x10},		{"DELETE",	0x10},

	/* 0xc8 */
	{"LOAD",	0x02},		{"LIST",	0x10},		{"NEW",		0x01},		{"OLD",		0x01},
	{"RENUMBER",0x10},		{"SAVE",	0x02},		{"",		0x00},		{"PTR",		0x00},

	/* 0xd0 */
	{"PAGE",	0x00},		{"TIME",	0x01},		{"LOMEM",	0x00},		{"HIMEM",	0x00},
	{"SOUND",	0x02},		{"BPUT",	0x03},		{"CALL",	0x02},		{"CHAIN",	0x02},

	/* 0xd8 */
	{"CLEAR",	0x01},		{"CLOSE",	0x03},		{"CLG",		0x01},		{"CLS",		0x01},
	{"DATA",	0x20},		{"DEF",		0x00},		{"DIM",		0x02},		{"DRAW",	0x02},

	/* 0xe0 */
	{"END",		0x01},		{"ENDPROC",	0x01},		{"ENVELOPE",0x02},		{"FOR",		0x02},
	{"GOSUB",	0x12},		{"GOTO",	0x12},		{"GCOL",	0x02},		{"IF",		0x02},

	/* 0xe8 */
	{"INPUT",	0x02},		{"LET",		0x04},		{"LOCAL",	0x02},		{"MODE",	0x02},
	{"MOVE",	0x02},		{"NEXT",	0x02},		{"ON",		0x02},		{"VDU",		0x02},

	/* 0xf0 */
	{"PLOT",	0x02},		{"PRINT",	0x02},		{"PROC",	0x0a},		{"READ",	0x02},
	{"REM",		0x20},		{"REPEAT",	0x00},		{"REPORT",	0x01},		{"RESTORE",	0x12},

	/* 0xf8 */
	{"RETURN",	0x01},		{"RUN",		0x01},		{"STOP",	0x01},		{"COLOUR",	0x02},
	{"TRACE",	0x12},		{"UNTIL",	0x02},		{"WIDTH",	0x02},		{"OSCLI",	0x02}
};

KeyWord *QuickTable[26*26];

/*

	Setup function, to establish contents of QuickTable, store strlens, etc

*/

//#define HashCode(str)	(str[0] < 'A' || str[0] > 'Z' || str[1] < 'A' || str[1] > 'Z') ? 0 : (((str[0] - 'A')*25 + (str[1] - 'A') + strlen(str))%(26*26))
#define HashCode(str)	(str[0] < 'A' || str[0] > 'Z' || str[1] < 'A' || str[1] > 'Z') ? 0 : ((str[0] - 'A')*26 + (str[1] - 'A'))

void SetupBASICTables()
{
	/* set QuickTable to empty */
	int c = 26*26;
	while(c--)
		QuickTable[c] = NULL;

	/* go through tokens, store strlens & populate QuickTable */
	for(c = 0; c < 0x80; c++)
	{
		if(KeyWordTable[c].StrLen = strlen(KeyWordTable[c].Name))
		{
			/* reject any symbols that have already appeared 0x40 places earlier in the table */
			if(c < 0x40 || strcmp(KeyWordTable[c].Name, KeyWordTable[c - 0x40].Name))
			{
				int Code = HashCode(KeyWordTable[c].Name);
				KeyWord **InsertPointer = &QuickTable[Code];
				while(*InsertPointer)
					InsertPointer = &(*InsertPointer)->Next;

				*InsertPointer = &KeyWordTable[c];
			}
		}
	}

	/*

		Go through QuickTable, sorting each branch by string length

		I'm an idiot, so I've used insertion sort!

	*/
	c = 26*26;
	while(c--)
		if(QuickTable[c] && QuickTable[c]->Next)
		{
			/* sort first by string length */
			KeyWord **Check = &QuickTable[c];
			unsigned int CurLength = (*Check)->StrLen;
			Check = &(*Check)->Next;
			while(*Check)
			{
				/* check if out of order */
				if((*Check)->StrLen > CurLength)
				{
					/* unlink */
					KeyWord *Takeout = *Check;
					*Check = (*Check)->Next;

					/* start at top of list, find correct insertion point */
					KeyWord **InsertPoint = &QuickTable[c];
					while((*InsertPoint)->StrLen >= Takeout->StrLen)
						InsertPoint = &(*InsertPoint)->Next;

					/* ...and insert */
					Takeout->Next = *InsertPoint;
					*InsertPoint = Takeout;
				}
				else
				{
					CurLength = (*Check)->StrLen;
					Check = &(*Check)->Next;
				}
			}
		}

/*	int Tabs[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int hc = 0;
	for(c = 0; c < 26*26; c++)
	{
		KeyWord *Count = QuickTable[c];
		if(Count)
		{
			hc = c;
			int cp = 0;
			while(Count)
			{
				cp++;
				Count = Count->Next;
			}
			Tabs[cp]++;
		}
		else
			Tabs[0]++;
	}
	printf("%d\n", hc);
	c = 10;
	while(c--)
	{
		printf("%d: %d\n", c, Tabs[c]);
	}*/
}

/*

	Little function to return an error string

*/
char *ErrorTable[] =
{
	"",
	"BASIC is not currently active",
	"Unable to open file for input",
	"Program too large",
	"Unable to open file for output",
	"Malformed BASIC program or not running BASIC",
	"BASIC program appears to run past the end of RAM"
};
char DynamicErrorText[256];

int ErrorNum;

char *GetBASICError()
{
	return ErrorNum >= 0 ? ErrorTable[ErrorNum] : DynamicErrorText;
}

/*

	Functions to export BASIC code, i.e. decode from tokenised form to plain text

*/

bool ExtractLine(FILE *output, Uint8 *Memory, Uint16 Addr, Uint8 LineL)
{
	int LineLength = (int)LineL;

	while(LineLength >= 0)
	{
		Uint8 ThisByte = Memory[Addr]; Addr++; LineLength--;
		if(ThisByte >= 0x80)
		{
			if(ThisByte == 0x8d) // then we're about to see a tokenised line number
			{
				Uint16 LineNumber;

				// decode weirdo tokenised line number format
				LineNumber = Memory[Addr+1]&0x3f;
				LineNumber |= (Memory[Addr+2]&0x3f) << 8;
				LineNumber |= (Memory[Addr]&0x0c) << 12;
				LineNumber |= (Memory[Addr]&0x30) << 2;
				LineNumber ^= 0x4040;

				Addr += 3;
				LineLength -= 3;

				fprintf(output, "%d", LineNumber);
			}
			else //ordinary keyword
			{
				fputs(KeyWordTable[ThisByte - 0x80].Name, output);
				
				if(KeyWordTable[ThisByte - 0x80].Flags & 0x20)
				{
					//copy to end of line without interpreting tokens}
					while(LineLength >= 0)
					{
						fputc(Memory[Addr], output); Addr++; LineLength--;
					}
					return true;
				}
			}
		}
		else
		{
			switch(ThisByte)
			{
				default: fputc(ThisByte, output); break;
				case '"':
					/* copy string literal... */
					fputc('"', output);
					while(Memory[Addr] != '"' && LineLength >= 0)
					{
						fputc(Memory[Addr], output);
						Addr++; LineLength--;
					}
					if(Memory[Addr] == '"')
					{
						fputc(Memory[Addr], output);
						Addr++; LineLength--;
					}
				break;
			}
		}
	}

	return (LineLength == -1) ? true : false;
}

bool ExportBASIC(char *Filename, Uint8 *Memory)
{
	ErrorNum = 0;
	FILE *output = fopen(Filename, "wt");

	if(!output)
	{
		ErrorNum = 4;
		return false;
	}

	/* get the value of PAGEâ start reading BASIC code from there */
	Uint16 Addr = Memory[0x18] << 8;

	if(Addr >= 32768 - 4)
		ErrorNum = 6;

	while(!ErrorNum)
	{
		/* character here should be \r */
		if(Memory[Addr] != 0x0d)
		{
			ErrorNum = 5;
			break;
		}
		Addr++;

		/* get line number, check if we've hit the end of BASIC */
		Uint16 LineNumber;
		LineNumber = (Memory[Addr] << 8) | Memory[Addr+1];
		Addr += 2;
		if(LineNumber & 0x8000) // if we've hit the end of the program, exit
			break;

		Uint8 LineLength = Memory[Addr]; Addr++;

		if(Addr+LineLength >= 32768 - 4)
		{
			ErrorNum = 6;
			break;
		}

		/* print line number */
		fprintf(output, "%5d", LineNumber);

		/* detokenise, etc */
		if(!ExtractLine(output, Memory, Addr, LineLength - 4))
			break;

		/* add a newline */
		fputc('\n', output);

		/* should process line here, but chicken out */
		Addr += LineLength - 4;
	}

	fclose(output);

	return ErrorNum ? false : true;
}

/*

	Functions to import BASIC code, i.e. tokenise from plain text

*/

#define AlphaNumeric(v)\
						(\
							(v >= 'a' && v <= 'z') ||\
							(v >= 'A' && v <= 'Z') ||\
							(v >= '0' && v <= '9')\
						)

char IncomingBuffer[9];
Uint8 Token, NextChar;
unsigned int IncomingPointer;
FILE *inputfile;
bool EndOfFile, NumberStart;
unsigned int NumberValue, NumberLength;
int CurLine;

Uint8 *Memory;
Uint16 Addr;

inline bool WriteByte(Uint8 value)
{
	if(Addr == 32768) {ErrorNum = 3; return false;}
	Memory[Addr++] = value;
	return true;
}

int my_fgetc(FILE *in)
{
	int r;
	do
	{
		r = fgetc(in);
	}
	while(r == '\r');
	if(r == '\n') CurLine++;
	return r;
}

void GetCharacter()
{
	if(IncomingPointer == 8)
	{
		/* shift, load into position [8] */
		int c = 0;
		while(c < 8)
		{
			IncomingBuffer[c] = IncomingBuffer[c+1];
			c++;
		}

		IncomingBuffer[8] = my_fgetc(inputfile);
	}
	else
	{
		IncomingBuffer[IncomingPointer] = my_fgetc(inputfile);
		IncomingPointer++;
	}

	if(feof(inputfile)) //if we've hit feof then the last char isn't anything
	{
		IncomingPointer--;
		if(!IncomingPointer)
		{
			EndOfFile = true;
			return;
		}
	}

	/* check for tokens, set flags accordingly. Be a bit dense about this for now! */
	Token = IncomingBuffer[0];
	int Code = HashCode(IncomingBuffer);
	KeyWord *CheckPtr = QuickTable[Code];

	while(CheckPtr)
	{
		if(IncomingPointer >= CheckPtr->StrLen && !strncmp(IncomingBuffer, CheckPtr->Name, CheckPtr->StrLen))
		{
			Token = (CheckPtr - KeyWordTable) + 0x80;
			NextChar = IncomingBuffer[CheckPtr->StrLen];
			break;
		}

		CheckPtr = CheckPtr->Next;
	}

	/* check if this is a number start */
	NumberStart = false;
	if(Token >= '0' && Token <= '9')
	{
		NumberStart = true;
		char *end;
		NumberValue = strtol(IncomingBuffer, &end, 10);
		NumberLength = end - IncomingBuffer;
	}
}

void EatCharacters(int n)
{
	/* shift left n places, decrease IncomingPointer */
	int c = 0;
	while(c < 9-n)
	{
		IncomingBuffer[c] = IncomingBuffer[c+n];
		c++;
	}
	IncomingPointer -= n;
	IncomingBuffer[IncomingPointer] = '\0';
	while(n--)		/* this is a quick fix: it causes lots of unnecessary token searches... */
		GetCharacter();
}

bool CopyStringLiteral()
{
	// eat preceeding quote
	WriteByte(IncomingBuffer[0]);
	EatCharacters(1);

	// don't tokenise anything until another quote is hit, keep eye out for things that may have gone wrong
	while(!ErrorNum && !EndOfFile && IncomingBuffer[0] != '"' && IncomingBuffer[0] != '\n')
	{
		WriteByte(IncomingBuffer[0]);
		EatCharacters(1);
	}

	if(IncomingBuffer[0] != '"') // stopped going for some reason other than a close quote
	{
		ErrorNum = -1;
		sprintf(DynamicErrorText, "Malformed string literal on line %d", CurLine);
		return false;
	}

	// eat proceeding quote
	WriteByte(IncomingBuffer[0]);
	EatCharacters(1);

	return true;
}

bool DoLineNumberTokeniser()
{
	while(!ErrorNum && !EndOfFile)
	{
		if(NumberStart)
		{
			// tokenise line number
			Uint16 LineNumber = NumberValue ^ 0x4040;

			WriteByte(0x8d);

			WriteByte(((LineNumber&0xc0) >> 2) | ((LineNumber&0xc000) >> 12) | 0x40);
			WriteByte((LineNumber&0x3f) | 0x40);
			WriteByte(((LineNumber >> 8)&0x3f) | 0x40);

			EatCharacters(NumberLength);
		}
		else
			switch(Token)
			{
				// whitespace and commas do not cause this mode to exit
				case ' ':
				case ',':
					WriteByte(Token);
					EatCharacters(1);
				break;

				// hex numbers get through unscathed too
				case '&':
					WriteByte(Token);
					EatCharacters(1);

					while(
						!ErrorNum &&
						!EndOfFile &&
						(
							(IncomingBuffer[0] >= '0' && IncomingBuffer[0] <= '9') ||
							(IncomingBuffer[0] >= 'A' && IncomingBuffer[0] <= 'F')
						)
					)
					{
						WriteByte(IncomingBuffer[0]);
						EatCharacters(1);
					}
				break;

				/* grab strings without tokenising numbers */
				case '"':
					if(!CopyStringLiteral())
						return false;
				break;

				/* default action is to turn off line number tokenising and get back to normal */
				default: return true;
			}
	}
	return true;
}

bool EncodeLine()
{
	bool StartOfStatement = true;

	/* continue until we hit a '\n' or file ends */
	while(!EndOfFile && Token != '\n' && !ErrorNum)
	{
		/* even if this looks like a keyword, it really isn't if the conditional flag is set & the next char is alphanumeric*/
		if(
			Token >= 0x80 &&
			(KeyWordTable[Token - 0x80].Flags&1) &&
			AlphaNumeric(NextChar)
			)
			Token = IncomingBuffer[0];

		if(Token < 0x80)	//if not a keyword token
		{
			switch(Token)
			{
				default:	//default is dump character to memory
					WriteByte(Token);

					if(Token == ':') // a colon always switches the tokeniser back to "start of statement" mode
						StartOfStatement = true;

					// grab entire variables rather than allowing bits to be tokenised
					if
					(
						(Token >= 'a' && Token <= 'z') ||
						(Token >= 'A' && Token <= 'Z')
					)
					{
						StartOfStatement = false;
						EatCharacters(1);
						while(AlphaNumeric(IncomingBuffer[0]))
						{
							WriteByte(IncomingBuffer[0]);
							EatCharacters(1);
						}
					}
					else
						EatCharacters(1);

				break;
				case '*':
					WriteByte(Token);
					EatCharacters(1);

					if(StartOfStatement)
					{
						/* * at start of statement means don't tokenise rest of statement, other than string literals */
						while(!EndOfFile && !ErrorNum && IncomingBuffer[0] != ':' && IncomingBuffer[0] != '\n')
						{
							switch(IncomingBuffer[0])
							{
								default:
									WriteByte(IncomingBuffer[0]);
									EatCharacters(1);
								break;
								case '"':
									if(!CopyStringLiteral())
										return false;
								break;
							}
						}
					}
				break;
				case '"':
					if(!CopyStringLiteral())
						return false;
				break;
			}
		}
		else
		{
			Uint8 Flags = KeyWordTable[Token - 0x80].Flags; //make copy of flags, as we're about to throwaway Token

			WriteByte(Token);	//write token
			EatCharacters(KeyWordTable[Token - 0x80].StrLen);

			/*
			
				Effect token flags
			
			*/
			if(Flags & 0x08)
			{
				/* FN or PROC, so duplicate next set of alphanumerics without thought */
				while(!ErrorNum && !EndOfFile && AlphaNumeric(IncomingBuffer[0]))
				{
					WriteByte(IncomingBuffer[0]);
					EatCharacters(1);
				}
			}

			if(Flags & 0x10)
			{
				/* tokenise line numbers for a bit */
				if(!DoLineNumberTokeniser())
					return false;
			}

			if(Flags & 0x20)
			{
				/* REM or DATA, so copy rest of line without tokenisation */
				while(!ErrorNum && !EndOfFile && IncomingBuffer[0] != '\n')
				{
					WriteByte(IncomingBuffer[0]);
					EatCharacters(1);
				}
			}

			if(
				(Flags & 0x40) && StartOfStatement
			)
			{
				/* pseudo-variable flag */
				Memory[Addr-1] += 0x40;	//adjust just-written token
			}

			/* check if we now go into middle of statement */
			if(Flags & 0x02)
				StartOfStatement = false;

			/* check if we now go into start of statement */
			if(Flags & 0x04)
				StartOfStatement = true;
		}
	}

	EatCharacters(1);	//either eat a '\n' or have no effect at all

	return true;
}

bool ImportBASIC(char *Filename, Uint8 *Mem)
{
	/* store memory target to global var */
	Memory = Mem;
	ErrorNum = 0;

	/* get the value of PAGEâ insert BASIC code starting from there */
	Addr = Memory[0x18] << 8;

	/* validity check: does PAGE currently point to a 0x0d? */
	if(Memory[Addr] != 0x0d)
	{
		ErrorNum = 1;
		return false;
	}

	/* validity check: does TOP - 2 point to a 0x0d, 0xff? */
	Uint16 TOPAddr = Memory[0x12] | (Memory[0x13] << 8);
	if(
		(Memory[TOPAddr-2] != 0x0d) ||
		(Memory[TOPAddr-1] != 0xff)
	)
	{
		ErrorNum = 1;
		return false;
	}

	/* open file, reset variables */
	inputfile = fopen(Filename, "rt");
	IncomingPointer = 0;
	CurLine = 1;
	EndOfFile = false;

	if(!inputfile)
	{
		ErrorNum = 2;
		return false;
	}

	/* fill input buffer */
	int c = 8;
	while(c--)
		GetCharacter();

	while(!EndOfFile && !ErrorNum)
	{
		/* get line number */
			/* skip white space and empty lines */
			while(Token == ' ' || Token == '\t' || Token == '\r' || Token == '\n')
				EatCharacters(1);
				
			/* end of file? */
			if(EndOfFile) break;

			/* now we should see a line number */
			if(!NumberStart || (NumberValue >= 32768))
			{
				ErrorNum = -1;
				sprintf(DynamicErrorText, "Malformed line number at line %d", CurLine);
				break;
			}

			/* inject into memory */
			WriteByte(0x0d);
			WriteByte(NumberValue >> 8);
			WriteByte(NumberValue&0xff);
			EatCharacters(NumberLength);

		/* read rest of line, record length */
		Uint16 LengthAddr = Addr; WriteByte(0);
		if(!EncodeLine())
			break;
		Memory[LengthAddr] = (Uint8)(Addr - LengthAddr + 3);
	}

	/* write "end of program" */
	WriteByte(0x0d);
	WriteByte(0xff);
	
	/* write TOP */
	Memory[0x12] = Addr&0xff;
	Memory[0x13] = Addr >> 8;

	fclose(inputfile);
	return ErrorNum ? false : true;
}
