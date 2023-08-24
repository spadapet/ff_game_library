#pragma once
#pragma push_macro("assert")

// C++
#include <fstream>

// FF
#pragma pop_macro("assert")
#include <ff.audio.h>
#include <ff.ui.h>

// WinRT
#if UWP_APP
#include <winrt/Windows.ApplicationModel.h>
#endif
