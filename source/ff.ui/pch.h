#pragma once
#pragma push_macro("assert")

// Noesis
#include <NoesisPCH.h>
#define NS_APP_INTERACTIVITY_API
#include <NsApp/TargetedTriggerAction.h>

// WinRT
#if UWP_APP
#include <winrt/Windows.System.h>
#endif

// FF
#pragma pop_macro("assert")
#include <ff.input.h>
#include <ff.graphics.h>
