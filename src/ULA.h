#ifndef __ULA_H
#define __ULA_H

#include "ComponentBase.h"
#include "ProcessPool.h"

#define ULAIRQ_MASTER		0x01
#define ULAIRQ_POWER		0x02
#define ULAIRQ_DISPLAY		0x04
#define ULAIRQ_RTC			0x08
#define ULAIRQ_RECEIVE		0x10
#define ULAIRQ_SEND			0x20
#define ULAIRQ_HTONE		0x40

#define MEM_RAM				0
#define MEM_SHADOW			1
#define MEM_ROM(n)			2+n

#define CULA_AUDIOEVENT_LENGTH	1024

#define KEYMOD_SHIFT	4
#define KEYMOD_CTRL		2
#define KEYMOD_ALT		1

#define ROMMODE_OFF		0
#define ROMMODE_ROM		1
#define ROMMODE_RAM		2

#define ROMSLOT_BASIC	8

#define HALTING_NONE	0
#define HALTING_NORMAL	1
#define HALTING_MODE3	2

enum ULAREG{ULAREG_INTSTATUS, ULAREG_INTCONTROL, ULAREG_LASTPAGED, ULAREG_PAGEREGISTER};

class CULA : public CComponentBase
{
	public:
		CULA(ElectronConfiguration &cfg);
		virtual ~CULA();

		/* this one returns the places in 6502 memory where different things are placed, e.g. MEM_RAM, MEM_SHADOW, etc */
		int GetMappedAddr(int area);

		void AttachTo(CProcessPool &pool, Uint32 id);
		bool Write(Uint16 Addr, Uint32 TimeStamp, Uint8 Data8, Uint32 Data32);
		bool Read(Uint16 Addr, Uint32 TimeStamp, Uint8 &Data8, Uint32 &Data32);
		bool IOCtl(Uint32 Control, void *Parameter = NULL, Uint32 TimeStamp = 0);

		Uint32 Update(Uint32 TotalCycles, bool);

		/* ULA specific */
		void AdjustInterrupts(Uint32 TimeStamp, Uint8 ANDMask = 0xff, Uint8 ORMask = 0);
		bool InterruptMeaningful(Uint8);
		void SetULARAMTiming(int HaltingMode);
		void SetMemoryModel(MRBModes Mode);

		/* keyboard related */

		/* these two are slightly obvious */
		bool LoadKeyMap(char *name);
		bool SaveKeyMap(char *name);

		/* SetDefaultKeyMap establishes the 'physical' mapping of ElectrEm days of yore */
		void SetDefaultKeyMap();

		/* see below for notes on this! */
		void RedefineKey(Uint8 SrcMods, SDLKey Key, Uint8 TrgMods, Uint8 Line, Uint8 Mask);

		/* UpdateKeyTable updates the Electron keyboard state based on the current PC keyboard state */
		void UpdateKeyTable();

		/* QueryKey returns true if the 'ElkCode' key is depressed, false otherwise.
		An ElkCode is formed with a high nibble equal to keyline, low nibble equal to mask */
		bool QueryKey(Uint8 ElkCode);

		/*
			NB:
				the usage of RedefineKey may not be immediately obvious. ElectrEm uses
				a keyboard mapping system which relies upon the idea of modifier keys -
				i.e. shift, ctrl and alt.

				When a PC key is pressed, ElectrEm does a little table lookup to decide
				which Electron key that means is pressed, and which Electron modifiers are
				simultaneously depressed for that PC key.

				However, it is also possible to nominate keys as PC modifiers. Which PC
				modifiers are pressed helps ElectrEm select which table to look at.

				Now, usage of RedefineKey:

					SrcMods -	a bitfield OR'd together from KEYMOD_SHIFT, KEYMOD_ALT &
								KEYMOD_CTRL determining which PC modifiers need to be
								pressed for this interpretation to take effect.

					Key -		the PC key being redefined. This will be one of the SDLK_?
								defines, from SDL_keysym.h

					TrgMods -	the Electron modifiers that will appear to be pressed
								whilst this key is

					Line -		the Electron keyline that this key should pretend to rest
								upon. See the table below.

					Mask -		the bit mask that this key contributes to that key line

					SPECIAL USAGES:

						pass TrgMods = Mask = 0 to define a PC key as not affecting
						the electron keyboard state

						pass Mask = 0, TrgMods != 0 to define a key as being a PC-wise
						modifier

					Key Table:

						Line	Mask	Key

						0		1		cursor right
						0		2		copy
						0		8		space bar

						1		1		cursor left
						1		2		cursor down
						1		4		enter
						1		8		delete

						2		1		minus
						2		2		cursor up
						2		4		colon

						3		1		0
						3		2		p
						3		4		semi-colon
						3		8		forward slash

						4		1		9
						4		2		o
						4		4		l
						4		8		full stop

						5		1		8
						5		2		i
						5		4		k
						5		8		comma

						6		1		7
						6		2		u
						6		4		j
						6		8		m

						7		1		6
						7		2		y
						7		4		h
						7		8		n

						8		1		5
						8		2		t
						8		4		g
						8		8		b

						9		1		4
						9		2		r
						9		4		f
						9		8		v

						10		1		3
						10		2		e
						10		4		d
						10		8		c

						11		1		2
						11		2		w
						11		4		s
						11		8		x

						12		1		1
						12		2		q
						12		4		a
						12		8		z
						
						13		1		escape
						13		2		func
						13		4		ctrl
						13		8		shift

					Bonus keys:

						ElectrEm defines a few bonus keys not present on the Electron
						keyboard. These all lie on keyboard lines which are not
						accessible to the Electron hardware, and are ideologically included
						here for convenience. They are as follows:

						Line	Mask	Key

						14		1		quit emulator
						14		2		enter/exit GUI
						14		4		full screen toggle
						14		8		super-hard reset (i.e. power off and on again)

						15		1		reset line (i.e. BREAK key)

						15		1		first byte joystick up
						15		2		first byte joystick down
						15		4		first byte joystick left
						15		8		first byte joystick right
						15		16		first byte joystick fire button

						16		1		plus 1 first joystick up
						16		2		plus 1 first joystick down
						16		4		plus 1 first joystick left
						16		8		plus 1 first joystick right
						16		16		plus 1 first joystick fire button

						17		1		plus 1 second joystick up
						17		2		plus 1 second joystick down
						17		4		plus 1 second joystick left
						17		8		plus 1 second joystick right
						17		16		plus 1 second joystick fire button

						NB plus 1 joysticks are treated as digital for this purpose
			*/

			/* for programming the keyboard to do a particular thing */
			void BeginKeySequence();
				void PressKey(Uint8 Line, Uint8 Code);
				void ReleaseKey(Uint8 Line, Uint8 Code);
				void KeyWait(Uint32 Cycles);
				void TypeASCII(const char *str);
			void EndKeySequence();

			/*
				ROM stuff
			*/
			void SetROMMode(int slot, int mode);
			int GetROMMode(int slot);
			bool InstallROM(char *name, int slot);
			
			/*
				stuff to allow state saves
			*/
			Uint8 QueryRegister(ULAREG register);
			Uint8 QueryRegAddr(Uint16 Addr);

	private:
		/* memory */
		Uint8 PagedRom, LastPageWrite, LastPage;
		Uint32 RomStates;
		int RomAddrs[16];
		int RamAddr, ShadowAddr, JimAddr, ScratchAddr;
		bool ROMClaimed;
		bool DoubleROM, NoROM, Keyboard, HaveCatch;
		int SecondROMNo;

		/* timing */
		Uint32 TotalTime;

		/* interrupt control stuff */
		Uint8 Status, StatusMask;

		void Page(Uint16 TargAddr, Uint32 SrcAddr, Uint16 Length, bool ReadOnly = true);
		/* keyboard stuff */
		bool CapsLED;

		Uint8 KeyboardState[16];
		struct KeyDefinition
		{
			Uint8 Shift, Line, Mask;
		} KeyTables[8][SDLK_LAST];

		/* keyboard program */
		struct KeyPress
		{
			Uint32 Wait;
			bool CapsDep;

			Uint8 Line;
			Uint8 MaskAnd, MaskOr;

			KeyPress *Next;
		} *KeyProgram, *KeyWorker, *LastKeyPress;
		Uint32 ProgramTime;
		KeyPress *GetNewKeyBuffer();

		/* audio */
		SDL_AudioSpec AudioSpec;

		struct AudioEvent
		{
			enum
			{
				START, STOP, SET_DIVIDER, NOP
			} Type;

			Uint8 Value;
			Uint32 SampleDiff, ClockTime, Remainder;
		};

		AudioEvent AudioBuffer[CULA_AUDIOEVENT_LENGTH];
		Uint32 AudioWritePtr, AudioReadPtr;
		SDL_mutex *AudioReadPtrMutex;
		void AudioUpdateFunction(void *, Uint8 *, int);
		static void AudioUpdateFunctionHelper(void *, Uint8 *, int);
		void WriteAudioEvent(Uint32);

		Uint32 AudioPtr, AudioInc, AudioMask, AudioMaskBackup;
		Uint32 AudioProcessTime;
		Uint64 AudioNumerator;
		void EnactAudioEvent();
		bool AudioEnabled;

		/* backups */
		Uint8 ClockDividerBackup, ControlBackup;

		/* timing bits */
		Uint32 **OneMhzBus, **TwoMhzBus, **HaltingBus, **HaltingBusMode3;
		Uint32 *OneMhzRow, *TwoMhzRow, *HaltingRow;
		MRBModes MRBMode;
		int MemHalting;

		/* volume */
		Uint8 LowSoundLevel, Volume;
};

#endif
