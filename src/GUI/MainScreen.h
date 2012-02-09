#ifndef __MAINSCREEN_H
#define __MAINSCREEN_H

#include "GUIObjects.h"

class CGUIMainDlg: public CGUIObjectCollection
{
	public:
		CGUIMainDlg(CGUI &, SDL_Surface *Static, SDL_Rect &FullArea);
		~CGUIMainDlg();

		void Message(CGUIMessage *);

	private:
		GUIObjectPtr *Tabs;
		CGUIObjectBase *Child;
		int NumTabs;
		void BringToFore(int id);
		CGUIObjectBase *GetChild(int id);

		CGUI *Parent;
		SDL_Surface *TabLine;
		SDL_Rect TabLineTarget;

		SDL_Surface *Static;
		SDL_Rect SubArea;
};

#endif
