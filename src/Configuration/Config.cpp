/*

	ElectrEm (c) 2000-4 Thomas Harte - an Acorn Electron Emulator

	This is open software, distributed under the GPL 2, see 'Copying' for
	details

	Config.cpp
	==========

	Some simple code for config file reading. Almost unique in
	coming virtually unchanged from ElectrEm Classic (circa beta 7).

*/

#include "../HostMachine/HostMachine.h"
#include "Config.h"
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
/* CONSTRUCTOR / DESTRUCTOR */
CConfig::CConfig(void)
{
	List = Current = NULL;
	Req = NULL;
	FName = NULL;
	Dirty = false;
}

CConfig::~CConfig(void)
{
	Close();
}

void CConfig::Flush()
{
	FILE *out;

	/* save file here, if one is open */
	if( NULL == FName || !Dirty )
	{
		return;
	}

	out = fopen( FName, "wt" );
	if  ( NULL == out )
	{
		return;
	}

	Current = List;
	while(Current)
	{
		fputs(Current->Title, out);

		if(Current->Value.Type != CT_NONE)
		{
			fputs(" = ", out);

			switch(Current->Value.Type)
			{
				default: break;
				case CT_BOOLEAN:
					fputs(Current->Value.r.Bool ? "true" : "false", out);
				break;

				case CT_INTEGER:
					fprintf(out, "%d", Current->Value.r.Int);
				break;

				case CT_STRING:
					if(Current->Value.String)
						fputs(Current->Value.String, out);
				break;
			}
		}

		if(Current->Next)
			fputc('\n', out);

		Current = Current->Next;
	}

	fclose(out);
	Dirty = false;
}

CConfig::ConfigIndex *CConfig::NewIndex(void)
{
	if(!List)
	{
		Current = List = new ConfigIndex;
	}
	else
	{
		Current = List;
		while(Current->Next)
		{
			Current = Current->Next;
		}
		Current->Next = new ConfigIndex;
		Current = Current->Next;
	}

	Current->Value.Type = CT_NONE;
	Current->Title = NULL;
	Current->Next = NULL;
	Current->FreeStr = false;

	return Current;
}

void CConfig::GetNewLine(FILE *f)
{
	NewIndex();

	int len = 256, pos = 0, nextc;
	Current->Title = (char *)malloc(len);

	do
	{
		nextc = fgetc(f);

		if(nextc != '\r' && nextc != '\n' && nextc != EOF)
		{
			Current->Title[pos] = nextc;

			pos++;
			if(pos == len)
			{
				len += 256;
				Current->Title = (char *)realloc(Current->Title, len);
			}
		}
	}
	while(nextc != '\n' && nextc != EOF);

	Current->Title[pos] = '\0';
}

void CConfig::RollLeft(void)
{
	int c = 0, l;

	l = strlen(Current->Title);

	while(l--)
	{
		Current->Title[c] = Current->Title[c+1];
		c++;
	}
}

void CConfig::DetermineType( char *arg )
{
	char *end, *p, *pc;

	struct banswer
	{
		char *text;
		bool r;
	};
	banswer *b, banswers[] =
	{
		{"true", true},		{"yes", true},	{"on", true},
		{"false", false},	{"no", false},	{"off", false},
		{NULL}
	};

	//kill white space at end
	p = arg + strlen(arg) - 1;
	while(*p == ' ' || *p == '\t')
		p--;
	p[1] = '\0';

	//check if there actually is a setting here
	if(!strlen(arg))
	{
		Current->Value.Type = CT_NONE;
		return;
	}

	//check if this looks like a number
	Current->Value.r.Int = strtol(arg, &end, 0);
	if(*end == '\0')
	{
		Current->Value.Type = CT_INTEGER;
		return;
	}

	//now assume it is a string, but check for boolean values
	Current->Value.String = arg;
	Current->Value.Type = CT_STRING;

	pc = p = strdup(arg);
	while(*pc)
	{
		*pc = tolower(*pc);
		pc++;
	}

	b = banswers;
	while(b->text)
	{
		if(!stricmp(b->text, p))
		{
			Current->Value.Type = CT_BOOLEAN;
			Current->Value.r.Bool = b->r;
			return;
		}

		b++;
	}
}

void CConfig::FreeIndex(void)
{
	if(Current == List)
	{
		List = List->Next;
	}
	else
	{
		ConfigIndex *c = List;
		while(c->Next != Current)
			c = c->Next;
		c->Next = Current->Next;
	}

	if(Current->Title)
	{
		free(Current->Title);
		Current->Title = NULL;
	}

	if(Current->FreeStr && Current->Value.String)
	{
		free(Current->Value.String);
		Current->Value.String = NULL;
	}

	delete Current;
	Current = List;
}

void CConfig::Close(void)
{
	Flush();

	ConfigIndex *n;
	delete[] FName;
	FName = NULL;

	/* free memory */
	Current = List;
	while(Current)
	{
		n = Current->Next;
		FreeIndex();
		Current = n;
	}

	List = Current = NULL;

	if(Req)
	{
		free(Req);
		Req = NULL;
	}
}

bool CConfig::Open( const char *name)
{
	FILE *input;
	char *t;
	bool assign;

	Close();
	Dirty = false;

	FName = GetHost() -> ResolveFileName(name);
	if(input = fopen(FName, "rt"))
	{
		do
		{
			GetNewLine(input);

			//get rid of preceeding white space
			while(Current->Title[0] == ' ' || Current->Title[0] == '\t')
				RollLeft();

			//check now if is a comment
			if(Current->Title[0] == '#' || Current->Title[0] == '\0')
			{
				Current->Value.Type = CT_NONE;
			}
			else
			{
				//check an '=' is in there somewhere
				t = Current->Title;
				assign = false;
				while(*t)
				{
					if(*t == '=') assign = true;
					t++;
				}

				if(!assign)
				{
					Current->Value.Type = CT_NONE;
				}
				else
				{
					//find & mark end of identifier
					t = Current->Title;
					while(*t != ' ' && *t != '\t' && *t != '=')
						t++;

					*t = '\0'; //fix up title

					//negotiate assignment operator
					while(*t != '=')
						t++;
					t++;

					//get rid of white space preceeding setting
					while(*t == ' ' || *t == '\t')
						t++;

					DetermineType(t);
				}
			}
		}
		while(!feof(input));

		fclose(input);
	}

	return true;
}

bool CConfig::FindSetting( const char *name, ConfigType style)
{
	Found = false;
	Current = List;

	if(Req) free(Req);
	Req = strdup(name);

	while(Current)
	{
		if(	!stricmp(Current->Title, name) &&
			( (style == CT_ANY) || (style == Current->Value.Type) ))
		{
			Found = true;
			break;
		}

		Current = Current->Next;
	}

	return Found;
}

ConfigSetting CConfig::ReadSetting(void)
{
	ConfigSetting junk;

	return (Current != NULL) ? Current->Value : junk;
}

void CConfig::WriteSetting(ConfigSetting setting)
{
	Dirty = true;

	if(setting.Type == CT_STRING && !setting.String)
		return;

	if(Found)
	{
		free(Current->Title);
		if(Current->FreeStr)
			free(Current->Value.String);
		Current->FreeStr = false;
	}
	else
		NewIndex();

	Current->Title = strdup(Req);
	Current->Value = setting;

	if(Current->Value.Type == CT_STRING)
	{
		Current->Value.String = strdup(setting.String);
		Current->FreeStr = true;
	}
}
