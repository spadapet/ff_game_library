#pragma once
#pragma push_macro("assert")

// C++
#include <fstream>

// Vendor
#ifdef BUILDING_FF_APPLICATION
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>
#endif

// FF
#pragma pop_macro("assert")
#include <ff.dx.h>
