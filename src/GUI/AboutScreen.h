#ifndef __ABOUTSCREEN_H
#define __ABOUTSCREEN_H

#include "GUIObjects.h"

class CGUIAboutDlg: public CGUIObjectCollection
{
	public: CGUIAboutDlg(CGUI &, SDL_Surface *Static, SDL_Rect &FullArea);
};
#endif
