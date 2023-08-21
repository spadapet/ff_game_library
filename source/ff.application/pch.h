#pragma once
#pragma push_macro("assert")

// C++
#include <fstream>

// WinRT
#if UWP_APP
#include <winrt/Windows.ApplicationModel.h>
#endif

// FF
#pragma pop_macro("assert")
#include <ff.audio.h>
#include <ff.ui.h>
