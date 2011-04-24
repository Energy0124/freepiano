#pragma once

#pragma warning(disable: 4819)

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION         0x0800
#endif

#include <stdio.h>
#include <windows.h>

typedef unsigned char byte;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(o)	if(o) { (o)->Release(); (o) = NULL; }
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(o)	if(o) { delete (o); (o) = NULL; }
#endif

#define ARRAY_COUNT(a)	(sizeof(a) / sizeof(a[0]))
#define ARRAY_END(a)	(a + sizeof(a) / sizeof(a[0]))

#define APP_NAME	"FreePiano"