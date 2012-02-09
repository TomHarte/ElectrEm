#ifndef __GUIOBJECTS_H
#define __GUIOBJECTS_H

#include "GUI.h"

struct CGUIMessage
{
	enum
	{
		MOUSEOVER, BUTTONS, DRAW, EVENT, KEY
	} Type;

	int PositionX, PositionY;
	Uint8 Buttons;

	union
	{
		struct
		{
			int DeltaX, DeltaY;
			bool Inside;
		} Mouse;

		struct
		{
			SDL_Surface *Target;
			Uint32 flag;
		} Draw;

		struct
		{
			Uint8 Changes;
		} Button;

		struct
		{
			void *Target;
			void *Source;
			Uint32 Message;
		} Event;

		struct
		{
			SDL_KeyboardEvent *Key;
		} Key;
	} Data;
};

/*

	BASE CLASSES

*/
class CGUIObjectBase
{
	public:
		virtual ~CGUIObjectBase();
		virtual void Message(CGUIMessage *);
		virtual SDL_Rect GetArea();
		virtual void Move(int OffsetX, int OffsetY);
		virtual void MakeStatic(SDL_Surface *Static);
		virtual void ChangeColour(SDL_Color &NewColour);
	protected:
		SDL_Surface *DisplaySurface;
		SDL_Rect Target;
};

class CGUIObjectCollection: public CGUIObjectBase
{
	public:
		virtual void Message(CGUIMessage *);
		virtual ~CGUIObjectCollection();
		void Move(int OffsetX, int OffsetY);
		virtual void ChangeColour(SDL_Color &NewColour);

	protected:
		struct GUIObjectPtr{
			CGUIObjectBase *Object;
			GUIObjectPtr *Next;
		} *Head;
		int OffsetX, OffsetY;
		SDL_Rect SubArea;

		/* for list that'll be 'Move'd and to which messages will be propagated */
		void InitialiseList();
		void AddObject(CGUIObjectBase *);
		void ClearList();

		/* for any other user defined list */
		void InitialiseList(GUIObjectPtr *&);
		void AddObject(GUIObjectPtr *&, CGUIObjectBase *); /* adds to start */
		void ClearList(GUIObjectPtr *&, bool DeleteObjects = false);
};


/*

	TEXT DISPLAY

*/
enum TextType
{
	TT_CENTRE, TT_LEFT, TT_RIGHT
};

class CGUIObjectText: public CGUIObjectBase
{
	public:
	
		virtual ~CGUIObjectText();
		CGUIObjectText(CGUI &, SDL_Surface *Static, const char *text, int size, SDL_Color &Fore, SDL_Rect &Target, TextType Alignment);
		void ChangeColour(SDL_Color &NewColour);

	private:
		char *TString;
		TTF_Font *font;
};


/*

	LINE

*/
class CGUIObjectLine: public CGUIObjectBase
{
	public: CGUIObjectLine(SDL_Surface *Static, SDL_Color &Fore, int x1, int y1, int x2, int y2);
};

/*

	OUTLINE BOX

*/
class CGUIObjectOutline: public CGUIObjectCollection
{
	public: CGUIObjectOutline(SDL_Surface *Static, SDL_Color &Fore, SDL_Rect &Area);
};

/*

	[FILLED] RECTANGLE

*/

class CGUIObjectRectangle: public CGUIObjectBase
{
	public:
		CGUIObjectRectangle(SDL_Surface *Static, SDL_Color &Fore, SDL_Rect &Area);
		void Message(CGUIMessage *);
	private:
		SDL_Color Colour;
};

/*

	BUTTON

*/
class CGUIObjectButton: public CGUIObjectBase
{
	public:
		CGUIObjectButton(CGUIObjectBase *MessageTarget, Uint32 TargetID, CGUI &GParent, SDL_Surface *Static, const char *text, int size, SDL_Color &Fore, SDL_Rect &Targ, bool bordered);
		virtual ~CGUIObjectButton();
		void Message(CGUIMessage *);
		void Move(int OffsetX, int OffsetY);
		void ChangeColour(SDL_Color &NewColour);

	private:
		bool Pressed, Over;
		CGUIObjectLine *Up[4], *Down[4];
		CGUIObjectBase *Wording;
		CGUIObjectBase *MouseOver;

		CGUIMessage ClickMessage;
		CGUI *Parent;
};

/*

	ANALOGUE BAR

*/
class CGUIObjectAnalogue: public CGUIObjectCollection
{
	public:
		CGUIObjectAnalogue(CGUI &GParent, SDL_Surface *Static, SDL_Rect &Targ, bool Horizontal = false, CGUIObjectBase *Child = NULL);
		virtual ~CGUIObjectAnalogue();

		SDL_Rect GetVisibleArea();
		float GetValue();
		void SetValue(float);
		void Message(CGUIMessage *);

	private:
		float Value, Horizontal;
		bool CursorTied;
		CGUIObjectBase *Slider, *Child;
		int ScrollArea;
		SDL_Rect SliderTarget;
		TextType Alignment;
};

/*

	TEXT BOX (read only)

*/
class CGUIObjectTextBox: public CGUIObjectBase
{
	public:
		CGUIObjectTextBox(CGUI &GParent, SDL_Surface *Static, const char *text, int size, SDL_Color &TextColour, SDL_Color &Background, SDL_Rect &Targ, TextType Alignment);
		CGUIObjectTextBox(CGUI &GParent, SDL_Surface *Static, const char *text, int size, SDL_Color &TextColour, SDL_Rect &Targ, TextType Alignment);
		virtual ~CGUIObjectTextBox();

	private:
		char *RenderLine(SDL_Surface *s, char *start, int OffsetX, int OffsetY);
		void Render(SDL_Surface *s, int OffsetX = 0, int OffsetY = 0);
		char *Text;
		TTF_Font *font;
		SDL_Color FontColour, BackColour;
		bool Alpha;
		TextType Alignment;
		int MaxW;
};

/*

	GRAPHIC

*/
class CGUIObjectGraphic: public CGUIObjectBase
{
	public:
		CGUIObjectGraphic(CGUI &GParent, SDL_Surface *Static, SDL_Rect &Targ, int GraphicID);
		void Message(CGUIMessage *);
	private:
		CGUI *Parent;
		int GraphicID;
};

#endif
