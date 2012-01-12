#ifndef _CONFIG_H
#define _CONFIG_H

/* package name */
#define PACKAGE "pianobar"

#define VERSION			"2012.01.10-dev"
#define VERSION_WIN32	"2012.01.12-dev"

#ifdef _MSC_VER
#define INLINE			__forceinline
#define bar_strdup		_strdup
#define bar_snprintf	_snprintf
#define bar_strcasecmp	_stricmp
#else
#define INLINE			inline
#define bar_strdup		strdup
#define bar_snprintf	snprintf
#define bar_strcasecmp	strcasecmp
#endif

#endif /* _CONFIG_H */
