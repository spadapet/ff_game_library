#pragma once

// C++
#include <array>
#include <atomic>
#include <coroutine>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <forward_list>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>

// Windows
#include <Windows.h>
#include <ShlObj.h>

// WinRT
#if UWP_APP
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#endif
