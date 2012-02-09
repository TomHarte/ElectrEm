#include <stdlib.h>
#include <ctype.h>

#define stricmp(x,y)  __My_stricmp(x,y)

inline int __My_stricmp(const char *x, const char *y)
{
	while (*x && *y && tolower(*x) == tolower(*y))
	{
		if (*x != *y)
			return (tolower(*x) > tolower(*y) ? 1 : -1);
		x++;
		y++;
	}
	if (*x || *y)
		return (*x ? 1 : -1);
	return 0;
}
