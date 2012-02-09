/*

	ElectrEm (c) 2000-6 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	Keyboard.cpp
	============

	Keyboard code - handles mapping your PC keyboard through the currently
	loaded key map to the Electron keyboard, loading, saving and
	modification of key maps, and the virtual keypress buffer

*/

#include "ULA.h"
#include "zlib.h"
#include "ProcessPool.h"
#include <memory.h>
#include <string.h>

bool CULA::LoadKeyMap(char *name)
{
	gzFile input;

	if(!(input = gzopen(name, "rb"))) return false;

	int table = 8;
	while(table--)
	{
		int key = SDLK_LAST;
		while(key--)
		{
			KeyTables[table][key].Shift = gzgetc(input);
			KeyTables[table][key].Mask = gzgetc(input);
			KeyTables[table][key].Line = gzgetc(input);
		}
	}

	gzclose(input);

	return true;
}

bool CULA::SaveKeyMap(char *name)
{
	gzFile input;

	if(!(input = gzopen(name, "wb9"))) return false;

	int table = 8;
	while(table--)
	{
		int key = SDLK_LAST;
		while(key--)
		{
			gzputc(input, KeyTables[table][key].Shift);
			gzputc(input, KeyTables[table][key].Mask);
			gzputc(input, KeyTables[table][key].Line);
		}
	}

	gzclose(input);

	return true;
}

void CULA::SetDefaultKeyMap()
{
	/* clear everything */
	int table = 8;
	while(table--)
	{
		int key = SDLK_LAST;
		while(key--)
		{
			KeyTables[table][key].Shift = 0;
			KeyTables[table][key].Line = 0;
			KeyTables[table][key].Mask = 0;
		}
	}

#define SetKey(key, line, mask)\
	KeyTables[c][key].Line = line;\
	KeyTables[c][key].Mask = mask;\
	KeyTables[c][key].Shift = c << 1;

	int c = 8;
	while(c--)
	{
		SetKey(SDLK_SPACE, 0, 8);
		SetKey(SDLK_INSERT, 0, 2);
		SetKey(SDLK_RIGHT, 0, 1);

		SetKey(SDLK_BACKSPACE, 1, 8);
		SetKey(SDLK_RETURN, 1, 4);
		SetKey(SDLK_DOWN, 1, 2);
		SetKey(SDLK_LEFT, 1, 1);

		SetKey(SDLK_QUOTE, 2, 4);
		SetKey(SDLK_UP, 2, 2);
		SetKey(SDLK_MINUS, 2, 1);

		SetKey(SDLK_SLASH, 3, 8);
		SetKey(SDLK_SEMICOLON, 3, 4);
		SetKey(SDLK_p, 3, 2);
		SetKey(SDLK_0, 3, 1);

		SetKey(SDLK_PERIOD, 4, 8);
		SetKey(SDLK_l, 4, 4);
		SetKey(SDLK_o, 4, 2);
		SetKey(SDLK_9, 4, 1);

		SetKey(SDLK_COMMA, 5, 8);
		SetKey(SDLK_k, 5, 4);
		SetKey(SDLK_i, 5, 2);
		SetKey(SDLK_8, 5, 1);

		SetKey(SDLK_m, 6, 8);
		SetKey(SDLK_j, 6, 4);
		SetKey(SDLK_u, 6, 2);
		SetKey(SDLK_7, 6, 1);

		SetKey(SDLK_n, 7, 8);
		SetKey(SDLK_h, 7, 4);
		SetKey(SDLK_y, 7, 2);
		SetKey(SDLK_6, 7, 1);

		SetKey(SDLK_b, 8, 8);
		SetKey(SDLK_g, 8, 4);
		SetKey(SDLK_t, 8, 2);
		SetKey(SDLK_5, 8, 1);

		SetKey(SDLK_v, 9, 8);
		SetKey(SDLK_f, 9, 4);
		SetKey(SDLK_r, 9, 2);
		SetKey(SDLK_4, 9, 1);

		SetKey(SDLK_c, 10, 8);
		SetKey(SDLK_d, 10, 4);
		SetKey(SDLK_e, 10, 2);
		SetKey(SDLK_3, 10, 1);

		SetKey(SDLK_x, 11, 8);
		SetKey(SDLK_s, 11, 4);
		SetKey(SDLK_w, 11, 2);
		SetKey(SDLK_2, 11, 1);

		SetKey(SDLK_z, 12, 8);
		SetKey(SDLK_a, 12, 4);
		SetKey(SDLK_q, 12, 2);
		SetKey(SDLK_1, 12, 1);

		SetKey(SDLK_LSHIFT, 0, 0);
		SetKey(SDLK_RSHIFT, 0, 0);
		SetKey(SDLK_LCTRL, 0, 0);
		SetKey(SDLK_RCTRL, 0, 0);
//		SetKey(SDLK_CAPSLOCK, 0, 0);
		SetKey(SDLK_LALT, 0, 0);
		SetKey(SDLK_RALT, 0, 0);
		SetKey(SDLK_ESCAPE, 13, 1);

		/* reset & GUI */
		SetKey(SDLK_F12, 15, 1);
		#if !defined( MAC )
			SetKey(SDLK_F10, 15, 1);
			SetKey(SDLK_F11, 14, 2);
		#else
			// F10 and F11 are mapped to Expose on OS X, F11 is also unncessary as we don't use the gui
			SetKey(SDLK_F1, 15, 1);
		#endif
	}

#undef SetKey

	/* set 'normal' shifts */
	KeyTables[0][SDLK_LSHIFT].Shift = KEYMOD_SHIFT << 4;
	KeyTables[0][SDLK_RSHIFT].Shift = KEYMOD_SHIFT << 4;
	KeyTables[0][SDLK_LCTRL].Shift = KEYMOD_CTRL << 4;
	KeyTables[0][SDLK_RCTRL].Shift = KEYMOD_CTRL << 4;
#if !defined(MAC)
	// This causes caps lock to act like a ctrl lock on OS X at least.
	KeyTables[0][SDLK_CAPSLOCK].Shift = KEYMOD_CTRL << 4;
#endif
	KeyTables[0][SDLK_LALT].Shift = KEYMOD_ALT << 4;
	KeyTables[0][SDLK_RALT].Shift = KEYMOD_ALT << 4;

	/* set full screen toggle */
	KeyTables[KEYMOD_ALT][SDLK_RETURN].Shift = 0;
	KeyTables[KEYMOD_ALT][SDLK_RETURN].Mask = 4;
	KeyTables[KEYMOD_ALT][SDLK_RETURN].Line = 14;

	/* set hard reset */
	KeyTables[KEYMOD_ALT][SDLK_F12].Shift = 0;
	KeyTables[KEYMOD_ALT][SDLK_F12].Mask = 8;
	KeyTables[KEYMOD_ALT][SDLK_F12].Line = 14;

	/* set quit */
	KeyTables[KEYMOD_ALT][SDLK_F4].Shift = 0;
	KeyTables[KEYMOD_ALT][SDLK_F4].Mask = 1;
	KeyTables[KEYMOD_ALT][SDLK_F4].Line = 14;
}

bool CULA::QueryKey(Uint8 ElkCode)
{
	return (KeyboardState[ElkCode >> 4]&ElkCode&0xf) ? true : false;
}

void CULA::RedefineKey(Uint8 SrcShifts, SDLKey Key, Uint8 TrgShifts, Uint8 Line, Uint8 Mask)
{
	if(Mask)
	{
		KeyTables[ SrcShifts ][ Key ].Line = Line;
		KeyTables[ SrcShifts ][ Key ].Mask = Mask;
		KeyTables[ SrcShifts ][ Key ].Shift = TrgShifts << 1;
	}
	else
	{
		KeyTables[ 0 ][ Key ].Shift = TrgShifts << 4;
	}
}

void CULA::BeginKeySequence()
{
	ProgramTime = TotalTime;
	memset(KeyboardState, 0, 16);

	KeyPress *Ptr = GetNewKeyBuffer();
	Ptr->Line = 0;
	Ptr->MaskAnd = 0xff;
	Ptr->MaskOr = 0;
}

#define KEY_PERIOD (60000)

CULA::KeyPress *CULA::GetNewKeyBuffer()
{
	KeyPress *Ptr = KeyProgram;

	if(!KeyProgram)
	{
		Ptr = KeyProgram = new KeyPress;
	}
	else
	{
		while(Ptr->Next)
			Ptr = Ptr->Next;

		Ptr->Next = new KeyPress;
		Ptr = Ptr->Next;
	}

	Ptr->Next = NULL;
	Ptr->Wait = KEY_PERIOD;
	Ptr->CapsDep = false;
	return Ptr;
}

void CULA::PressKey(Uint8 Line, Uint8 Code)
{
	KeyWorker = GetNewKeyBuffer();

	KeyWorker->Line = Line;
	KeyWorker->MaskAnd = 0xff;
	KeyWorker->MaskOr = Code;

	/* the following was found by experimentation versus the Electron ROM routines, which were treated as a black box */
	KeyWait(0);
	LastKeyPress = KeyWorker;
}

void CULA::ReleaseKey(Uint8 Line, Uint8 Code)
{
	KeyWorker = GetNewKeyBuffer();

	KeyWorker->Line = Line;
	KeyWorker->MaskAnd = ~Code;
	KeyWorker->MaskOr = 0;
}

void CULA::KeyWait(Uint32 Cycles)
{
	KeyWorker->Wait = Cycles;
}

#define SetCtrl()		PressKey(13, 4); KeyWait(0);
#define ClearCtrl()		ReleaseKey(13, 4); KeyWait(0);
#define SetUpper()		PressKey(13, 8); KeyWorker->CapsDep = true; KeyWait(0);
#define SetLower()		ReleaseKey(13, 8); KeyWorker->CapsDep = true; KeyWait(0);
#define SetShift()		PressKey(13, 8); KeyWait(0);
#define ClearShift()	ReleaseKey(13, 8); KeyWait(0);

#define TapKey(a, b)	PressKey(a, b); ReleaseKey(a, b); NewTap = (a << 4) | b;

void CULA::TypeASCII(const char *Str)
{
	int LastTap = 0, NewTap = 0;

	while(*Str)
	{
		switch(*Str)
		{
			/* upper case alphabet */
			case 'A':	SetUpper();	TapKey(12, 4);	break;		case 'B':	SetUpper(); TapKey(8, 8);	break;
			case 'C':	SetUpper(); TapKey(10, 8);	break;		case 'D':	SetUpper();	TapKey(10, 4);	break;
			case 'E':	SetUpper();	TapKey(10, 2);	break;		case 'F':	SetUpper();	TapKey(9, 4);	break;
			case 'G':	SetUpper();	TapKey(8, 4);	break;		case 'H':	SetUpper();	TapKey(7, 4);	break;
			case 'I':	SetUpper();	TapKey(5, 2);	break;		case 'J':	SetUpper();	TapKey(6, 4);	break;
			case 'K':	SetUpper(); TapKey(5, 4);	break;		case 'L':	SetUpper();	TapKey(4, 4);	break;
			case 'M':	SetUpper();	TapKey(6, 8);	break;		case 'N':	SetUpper();	TapKey(7, 8);	break;
			case 'O':	SetUpper();	TapKey(4, 2);	break;		case 'P':	SetUpper();	TapKey(3, 2);	break;
			case 'Q':	SetUpper(); TapKey(12, 2);	break;		case 'R':	SetUpper(); TapKey(9, 2);	break;
			case 'S':	SetUpper(); TapKey(11, 4);	break;		case 'T':	SetUpper();	TapKey(8, 2);	break;
			case 'U':	SetUpper();	TapKey(6, 2);	break;		case 'V':	SetUpper(); TapKey(9, 8);	break;
			case 'W':	SetUpper(); TapKey(11, 2);	break;		case 'X':	SetUpper(); TapKey(11, 8);	break;
			case 'Y':	SetUpper();	TapKey(7, 2);	break;		case 'Z':	SetUpper(); TapKey(12, 8);	break;

			/* lower case alphabet */
			case 'a':	SetLower();	TapKey(12, 4);	break;		case 'b':	SetLower(); TapKey(8, 8);	break;
			case 'c':	SetLower(); TapKey(10, 8);	break;		case 'd':	SetLower();	TapKey(10, 4);	break;
			case 'e':	SetLower();	TapKey(10, 2);	break;		case 'f':	SetLower();	TapKey(9, 4);	break;
			case 'g':	SetLower();	TapKey(8, 4);	break;		case 'h':	SetLower();	TapKey(7, 4);	break;
			case 'i':	SetLower();	TapKey(5, 2);	break;		case 'j':	SetLower();	TapKey(6, 4);	break;
			case 'k':	SetLower(); TapKey(5, 4);	break;		case 'l':	SetLower();	TapKey(4, 4);	break;
			case 'm':	SetLower();	TapKey(6, 8);	break;		case 'n':	SetLower();	TapKey(7, 8);	break;
			case 'o':	SetLower();	TapKey(4, 2);	break;		case 'p':	SetLower();	TapKey(3, 2);	break;
			case 'q':	SetLower(); TapKey(12, 2);	break;		case 'r':	SetLower(); TapKey(9, 2);	break;
			case 's':	SetLower(); TapKey(11, 4);	break;		case 't':	SetLower();	TapKey(8, 2);	break;
			case 'u':	SetLower();	TapKey(6, 2);	break;		case 'v':	SetLower(); TapKey(9, 8);	break;
			case 'w':	SetLower(); TapKey(11, 2);	break;		case 'x':	SetLower(); TapKey(11, 8);	break;
			case 'y':	SetLower();	TapKey(7, 2);	break;		case 'z':	SetLower(); TapKey(12, 8);	break;
			
			/* numbers */
			case '1':	ClearShift(); TapKey(12, 1);	break;
			case '2':	ClearShift(); TapKey(11, 1);	break;
			case '3':	ClearShift(); TapKey(10, 1);	break;
			case '4':	ClearShift(); TapKey(9, 1);		break;
			case '5':	ClearShift(); TapKey(8, 1);		break;
			case '6':	ClearShift(); TapKey(7, 1);		break;
			case '7':	ClearShift(); TapKey(6, 1);		break;
			case '8':	ClearShift(); TapKey(5, 1);		break;
			case '9':	ClearShift(); TapKey(4, 1);		break;
			case '0':	ClearShift(); TapKey(3, 1);		break;

			/* shifted numbers */
			case '!':	SetShift(); TapKey(12, 1);	break;
			case '"':	SetShift(); TapKey(11, 1);	break;
			case '#':	SetShift(); TapKey(10, 1);	break;
			case '$':	SetShift(); TapKey(9, 1);	break;
			case '%':	SetShift(); TapKey(8, 1);	break;
			case '&':	SetShift(); TapKey(7, 1);	break;
			case '\'':	SetShift(); TapKey(6, 1);	break;
			case '(':	SetShift(); TapKey(5, 1);	break;
			case ')':	SetShift(); TapKey(4, 1);	break;
			case '@':	SetShift(); TapKey(3, 1);	break;

			/* misc. unshifted */
			case ';':	ClearShift(); TapKey(3, 4);	break;
			case ':':	ClearShift(); TapKey(2, 4);	break;
			case '/':	ClearShift(); TapKey(3, 8);	break;
			case '.':	ClearShift(); TapKey(4, 8);	break;
			case ',':	ClearShift(); TapKey(5, 8);	break;
			case '-':	ClearShift(); TapKey(2, 1);	break;

			/* misc shifted */
			case '*':	SetShift();	TapKey(2, 4);	break;
			case '+':	SetShift(); TapKey(3, 4);	break;
			default:	/* all unknown come out as ?s */
			case '?':	SetShift(); TapKey(3, 8);	break;
			case '>':	SetShift(); TapKey(4, 8);	break;
			case '<':	SetShift(); TapKey(5, 8);	break;
			case '=':	SetShift(); TapKey(2, 1);	break;
			
			/* things shifted on cursor keys */
			case '^':	SetShift(); TapKey(1, 1);	break;
			case '|':	SetShift(); TapKey(0, 1);	break;
			case '£':	SetShift(); TapKey(2, 2);	break;
			case '_':	SetShift(); TapKey(1, 2);	break;

			/* things ctrl'd on cursor keys */
			case '~':	SetCtrl(); TapKey(1, 1);	ClearCtrl(); break;
			case '\\':	SetCtrl(); TapKey(0, 1);	ClearCtrl(); break;
			case '{':	SetCtrl(); TapKey(2, 2);	ClearCtrl(); break;
			case '}':	SetCtrl(); TapKey(1, 2);	ClearCtrl(); break;
			
			/* things hidden on the copy key */
			case ']':	SetCtrl(); TapKey(0, 2);	ClearCtrl(); break;
			case '[':	SetShift(); TapKey(0, 2);	break;

			/* tab, which is ctrl + i */
			case '\t':	SetCtrl(); TapKey(5, 2);	ClearCtrl(); break;

			/* space and return, which don't care about shift */
			case ' ':	TapKey(0, 8);	break;
			case '\n':	TapKey(1, 4); 	break;
			case '\r':	break;
			
		}

		/* if the same (physical) key is pressed more than once in succession then ElectrEm needs to linger before pressing the key
		again so that a release is properly detected */
		if(LastTap == NewTap)
			LastKeyPress->Wait = KEY_PERIOD;

		LastTap = NewTap;
		Str++;
	}
}

void CULA::EndKeySequence()
{
}

void CULA::UpdateKeyTable()
{
	Uint8 OldLine14 = KeyboardState[14];
	SDL_PumpEvents();
	Uint8 *KeyArray = SDL_GetKeyState(NULL);

	if(KeyProgram)
	{
		/* do an early quit if the user has Escape pressed */
		if(KeyArray[SDLK_ESCAPE])
		{
			while(KeyProgram)
			{
				KeyPress *Next = KeyProgram->Next;
				delete KeyProgram;
				KeyProgram = Next;
			}
		}
		else
		{
			Uint32 Difference = TotalTime - ProgramTime;

			while(Difference > KeyProgram->Wait)
			{
				if(KeyProgram->CapsDep && CapsLED)
					KeyboardState[ KeyProgram->Line ]=
							(KeyboardState[KeyProgram->Line ]&(~KeyProgram->MaskOr)) |
							(~KeyProgram->MaskAnd);
				else
					KeyboardState[ KeyProgram->Line ]=
							(KeyboardState[KeyProgram->Line ]&KeyProgram->MaskAnd) |
							KeyProgram->MaskOr;

				ProgramTime += KeyProgram->Wait;
				Difference -= KeyProgram->Wait;

				KeyPress *Next = KeyProgram->Next;
				delete KeyProgram;
				KeyProgram = Next;

				if(!KeyProgram) break;
			}
		}
	}
	else
	{
		memset(KeyboardState, 0, 16);

#if defined( MAC )
		/* OS X: ignore anything done with the command key pressed */
		if(KeyArray[SDLK_LMETA] || KeyArray[SDLK_RMETA]) return;
#endif

		/* first of all determine which modifiers are depressed */
		Uint8 Modifiers = 0;
		int c = SDLK_LAST;
		while(c--)
		{
			if(KeyArray[c] && KeyTables[0][c].Shift&0xf0)
				Modifiers |= KeyTables[0][c].Shift >> 4;
		}

		/* now adjust KeyboardState */
		c = SDLK_LAST;
		while(c--)
		{
			if(KeyArray[c])
			{
				KeyboardState[ KeyTables[ Modifiers ][c].Line ] |= KeyTables[ Modifiers ][c].Mask;
				KeyboardState[ 13 ] |= KeyTables[ Modifiers ][c].Shift;
			}
		}
	}

	/* check if any special key is being pressed */
	if(KeyboardState[14]&1)
		PPPtr->Message(PPM_QUIT); //quit
	if((OldLine14^KeyboardState[14])&(~KeyboardState[14])&8)
		PPPtr->Message(PPM_HARDRESET); //hard reset (happens when key released)
	if((OldLine14^KeyboardState[14])&KeyboardState[14]&4)
		PPPtr->Message(PPM_FSTOGGLE); //fullscreen toggle (happens when key pressed)
	if((OldLine14^KeyboardState[14])&KeyboardState[14]&2)
		PPPtr->Message(PPM_GUI); //gui (happens when key pressed)

	PPPtr->IOCtl(IOCTL_SETRST, (KeyboardState[15]&1) ? this : NULL, TotalTime); //reset
}
