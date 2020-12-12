#pragma once

#if 0

namespace ff
{
	UTIL_API HANDLE CreateEvent(bool initialSet = false, bool manualReset = true);
	UTIL_API bool IsEventSet(HANDLE hEvent);
	UTIL_API bool WaitForEventAndReset(HANDLE handle);
	UTIL_API bool WaitForHandle(HANDLE handle);
	UTIL_API size_t WaitForAnyHandle(HANDLE* handles, size_t count);
}

#endif
