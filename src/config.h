# pragma once

/* package name */
#define PACKAGE "pianobar"

#define VERSION "2024.07.04-dev"

#define TITLE   "Pianobar"


/* Visual C++ name restrict differently */
#ifdef _WIN32
#define restrict __restrict
#endif

/* Visual C++ does not have inline keyword in C */
#ifdef _WIN32
#define inline __inline
#endif

/* Visual C++ does not have strcasecmp */
#ifdef _WIN32
#define strcasecmp _stricmp
#endif

#ifndef NDEBUG
#define HAVE_DEBUGLOG
#define debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug(...)
#endif

