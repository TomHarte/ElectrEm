#ifndef __GUI_H
#define __GUI_H

#include "SDL.h"
#include "SDL_ttf.h"
#include "../ProcessPool.h"
#include "../UEF.h"

struct CGUIMessage;
class CGUIObjectBase;

class CGUI
{
	public:
		CGUI(CProcessPool *, BasicConfigurationStore *);
		~CGUI();
		bool Go(); /* returns true if emulator should close down, false otherwise */
		void SignalQuit(bool QuitEmulator = false);

		TTF_Font *GetFont(int size);
		void AddGraphic(SDL_Surface *s, int GraphicID, int x, int y);
		SDL_Rect GetGraphicDimensions(int GraphicID);
		CProcessPool *GetProcessPool();
		BasicConfigurationStore *GetConfigFile();

		void PostMessage(CGUIMessage *);

	private:
		CProcessPool *PPool;
		BasicConfigurationStore *ConfigFile;
		volatile bool Quit;

		SDL_Surface *StaticBuffer;
		TTF_Font *Font12;
		TTF_Font *Font14;
		TTF_Font *Font18;
		TTF_Font *Font24;

		void InstallResource(int id, char *image, char *mask = NULL);
		bool GetImage(CUEFChunkSelector *sel, int id);
		SDL_Surface *SrcResources[256], *Resources[256];
		void MapResources();
		void FreeResources();

		CGUIObjectBase *GUIObject;

		CGUIMessage *MessageQueue;
		int MessagesPending;

		bool QMessage;
};

/* one terribly useful function */
extern SDL_Surface *OptimiseSurface(SDL_Surface *, bool alpha = false);
extern SDL_Surface *OptimalSurface(int w, int h, bool alpha = false);

#endif
