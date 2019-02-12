#pragma once

#define XI_STDCALL __stdcall
#define XI_CDECL __cdecl
#define XI_FASTCALL __fastcall
#define XI_PASCAL __pascal

typedef char xbool;
#define true (char)1
#define false (char)0
typedef const char * xstring;
typedef void* xptr;

#if  defined(WIN32)||defined(_DRIVER)
typedef 	__int32				xint;
typedef 	signed __int64		xlong;
typedef		unsigned char		xbyte;
typedef 	char 				xchar;
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
typedef 	int32_t				xint;
typedef 	signed long long	xlong;
typedef		unsigned char		xbyte;
typedef 	char 				xchar;
#endif