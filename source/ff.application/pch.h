#pragma once
#pragma push_macro("assert")

// C++
#include <fstream>

// Noesis
#include <NoesisPCH.h>

// Duplicated in ff.vendor.noesis_app.vcxproj
#define NS_APP_INTERACTIVITY_API
#define NS_APP_MEDIAELEMENT_API
#define NS_APP_RIVE_API
#define NS_APP_RIVEBASE_API

// FF
#pragma pop_macro("assert")
#include <ff.dx.h>
#include <ff.graphics.h>
