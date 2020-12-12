#include "pch.h"
#include "util.h"

#if 0

#include "Globals/ProcessGlobals.h"
#include "Module/Module.h"
#include "Thread/ThreadDispatch.h"
#include "Thread/ThreadUtil.h"
#include "Windows/Handles.h"
#include "Windows/WinUtil.h"

HANDLE ff::CreateEvent(bool initialSet, bool manualReset)
{
	return ::CreateEventEx(nullptr, nullptr,
		(initialSet ? CREATE_EVENT_INITIAL_SET : 0) |
		(manualReset ? CREATE_EVENT_MANUAL_RESET : 0),
		EVENT_ALL_ACCESS);
}

bool ff::IsEventSet(HANDLE handle)
{
	assertRetVal(handle, true);

	return ::WaitForSingleObjectEx(handle, 0, FALSE) == WAIT_OBJECT_0;
}

bool ff::WaitForEventAndReset(HANDLE handle)
{
	noAssertRetVal(ff::WaitForHandle(handle), false);
	return ::ResetEvent(handle) != FALSE;
}

bool ff::WaitForHandle(HANDLE handle)
{
	return ff::WaitForAnyHandle(&handle, 1) == 0;
}

size_t ff::WaitForAnyHandle(HANDLE* handles, size_t count)
{
	ff::IThreadDispatch* threadDispatch = ff::ThreadGlobals::Exists()
		? ff::ThreadGlobals::Get()->GetDispatch()
		: nullptr;

	if (threadDispatch)
	{
		return threadDispatch->WaitForAnyHandle(handles, count);
	}

	while (true)
	{
		DWORD result = ::WaitForMultipleObjectsEx((DWORD)count, handles, FALSE, INFINITE, TRUE);
		switch (result)
		{
		default:
			if (result < count)
			{
				return result;
			}
			else if (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + count)
			{
				return ff::INVALID_SIZE;
			}
			break;

		case WAIT_TIMEOUT:
		case WAIT_FAILED:
			return ff::INVALID_SIZE;

		case WAIT_IO_COMPLETION:
			break;
		}
	}
}

#endif
