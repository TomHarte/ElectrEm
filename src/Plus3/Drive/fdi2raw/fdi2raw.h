#ifndef __FDI2RAW_H
#define __FDI2RAW_H

/*

	UAE original modified by Thomas Harte (thomas.harte [at] gmail.com) in 2005 to remove all
	'zfile' references & switch to zlib. FDI spec doesn't allow compression for FDIs anyway,
	so this should break nothing. FDI.gz will be supported.

*/

#include "SDL.h"
typedef Uint16 uae_u16;
typedef Uint8 uae_u8;
typedef Uint32 uae_u32;
#define xmalloc malloc
#define STATIC_INLINE

#include "zlib.h"
typedef struct fdi FDI;

#ifdef __cplusplus
extern "C" {
#endif

extern int fdi2raw_loadtrack (FDI*, uae_u16 *mfmbuf, uae_u16 *tracktiming, int track, int *tracklength, int *indexoffset, int *multirev, int mfm);
extern int fdi2raw_loadrevolution (FDI*, uae_u16 *mfmbuf, uae_u16 *tracktiming, int track, int *tracklength, int mfm);
extern FDI *fdi2raw_header(gzFile f);
extern void fdi2raw_header_free (FDI *);
extern int fdi2raw_get_last_track(FDI *);
extern int fdi2raw_get_num_sector (FDI *);
extern int fdi2raw_get_last_head(FDI *);
extern int fdi2raw_get_type (FDI *);
extern int fdi2raw_get_bit_rate (FDI *);
extern int fdi2raw_get_rotation (FDI *);
extern int fdi2raw_get_write_protect (FDI *);

#ifdef __cplusplus
}
#endif

#endif
