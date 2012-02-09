#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdio.h>

enum ConfigType
{
	CT_BOOLEAN, CT_INTEGER, CT_STRING, CT_ANY, CT_NONE
};

struct ConfigSetting
{
	union
	{
		bool Bool;
		int Int;
	} r;

	char *String;
	ConfigType Type;
};

class CConfig
{
	public :
		CConfig(void);
		~CConfig(void);

		bool Open( const char *name);
		void Close(void);

		void Flush();

		bool FindSetting( const char *name, ConfigType style = CT_ANY);
		ConfigSetting ReadSetting(void);
		void WriteSetting(ConfigSetting setting);

	private :
		struct ConfigIndex
		{
			ConfigIndex *Next;

			char *Title;
			ConfigSetting Value;
			bool FreeStr;
		};

		ConfigIndex *NewIndex(void);
 		void FreeIndex(void);
		void GetNewLine(FILE *f);
		void RollLeft(void);
		void DetermineType( char *arg);

 		ConfigIndex *List, *Current;
		bool Found, Dirty;

		char *Req, *FName;
};

#endif
