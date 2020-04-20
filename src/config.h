# pragma once

/* package name */
#define PACKAGE "pianobar"

#define VERSION "2020.04.20"

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

#define CURL_STATICLIB

