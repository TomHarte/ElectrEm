#ifndef __FILESCREEN_H
#define __FILESCREEN_H

#include "GUIObjects.h"
#include "../HostMachine/HostMachine.h"

class CGUIFileDlg: public CGUIObjectCollection
{
	public:
		CGUIFileDlg(CGUI &, SDL_Surface *Static, SDL_Rect &FullArea);
		~CGUIFileDlg();
		void Message(CGUIMessage *Msg);
	private:
		CGUI *Parent;
		class CGUIFileDlgContents *Cont;
		char *CurrentPath;
		SDL_Surface *Static;

		void SetToPath(char *name);
		FileDesc *FD;
};
#endif
