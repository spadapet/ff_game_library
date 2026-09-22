#pragma once

#include <intrin.h>
#include <math.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WINDOWS
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS // COM interfaces are called through their C macros, not C++ vtable syntax
#include <Windows.h>
#include <shlobj.h>
#include <shellscalingapi.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "synchronization.lib")
#endif
