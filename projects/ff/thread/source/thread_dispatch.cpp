#include "pch.h"
#include "thread_dispatch.h"

#if 0

#include "COM/ComAlloc.h"
#include "Globals/AppGlobals.h"
#include "Globals/ThreadGlobals.h"
#include "Globals/ProcessGlobals.h"
#include "Thread/ThreadDispatch.h"
#include "Thread/ThreadUtil.h"
#include "Windows/Handles.h"
#include "Windows/WinUtil.h"

class __declspec(uuid("d4f237b8-f9cb-4016-926f-349086fb41d7"))
	ThreadDispatch
	: public ff::ComBase
	, public ff::IThreadDispatch
#if !METRO_APP
	, public ff::IWindowProcListener
#endif
{
public:
	DECLARE_HEADER(ThreadDispatch);

	bool Init();

	// IThreadDispatch
	virtual void Post(std::function<void()>&& func, bool runIfCurrentThread) override;
	virtual void Send(std::function<void()>&& func) override;
	virtual bool IsCurrentThread() const override;
	virtual DWORD GetThreadId() const override;
	virtual void Flush() override;
	virtual void Destroy() override;
	virtual size_t WaitForAnyHandle(const HANDLE* handles, size_t count) override;
	virtual bool WaitForAllHandles(const HANDLE* handles, size_t count) override;

#if !METRO_APP
	// IWindowProcListener
	virtual bool ListenWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& nResult) override;
#endif

private:
	IThreadDispatch* InternalPost(std::function<void()>&& func, bool runIfCurrentThread);
	void Flush(bool force);
	void PostFlush();

	struct Entry
	{
		Entry()
		{
		}

		Entry(std::function<void()>&& func)
			: _func(std::move(func))
		{
		}

		std::function<void()> _func;
	};

	DWORD _threadId;
	ff::Mutex _mutex;
	ff::Vector<Entry, 32> _funcs;
	ff::WinHandle _flushedEvent;
	ff::WinHandle _pendingEvent;
#if METRO_APP
	Windows::UI::Core::CoreDispatcher^ _dispatcher;
	Windows::UI::Core::DispatchedHandler^ _handler;
#else
	ff::ListenedWindow _window;
#endif
};

BEGIN_INTERFACES(ThreadDispatch)
	HAS_INTERFACE(ff::IThreadDispatch)
END_INTERFACES()

ff::ComPtr<ff::IThreadDispatch> ff::CreateCurrentThreadDispatch()
{
	ComPtr<ThreadDispatch> myObj;
	assertHrRetVal(ff::ComAllocator<ThreadDispatch>::CreateInstance(&myObj), false);
	assertRetVal(myObj->Init(), nullptr);
	return myObj.Interface();
}

ff::IThreadDispatch* ff::GetThreadDispatch()
{
	ff::IThreadDispatch* dispatch = ff::GetCurrentThreadDispatch();

	// Try the main thread
	if (!dispatch && ff::ProcessGlobals::Exists())
	{
		dispatch = ff::ProcessGlobals::Get()->GetDispatch();
	}

	if (dispatch && dispatch->GetThreadId())
	{
		return dispatch;
	}

	return nullptr;
}

ff::IThreadDispatch* ff::GetCurrentThreadDispatch()
{
	ff::IThreadDispatch* dispatch = nullptr;

	if (ff::ThreadGlobals::Exists())
	{
		dispatch = ff::ThreadGlobals::Get()->GetDispatch();
	}

	if (dispatch && dispatch->GetThreadId())
	{
		return dispatch;
	}

	return nullptr;
}

ff::IThreadDispatch* ff::GetMainThreadDispatch()
{
	ff::IThreadDispatch* dispatch = nullptr;

	if (ff::ProcessGlobals::Exists())
	{
		dispatch = ff::ProcessGlobals::Get()->GetDispatch();
	}

	if (dispatch && dispatch->GetThreadId())
	{
		return dispatch;
	}

	return nullptr;
}

ff::IThreadDispatch* ff::GetGameThreadDispatch()
{
	ff::AppGlobals* globals = ff::AppGlobals::Get();
	return globals ? globals->GetGameDispatch() : ff::GetMainThreadDispatch();
}

ThreadDispatch::ThreadDispatch()
	: _threadId(0)
{
}

ThreadDispatch::~ThreadDispatch()
{
	assertSz(!_threadId, L"The owner of ThreadDispatch should've called Destroy() by now");
}

bool ThreadDispatch::Init()
{
	_threadId = ::GetCurrentThreadId();
	_flushedEvent = ff::CreateEvent(true);
	_pendingEvent = ff::CreateEvent();

#if METRO_APP
	Windows::UI::Core::CoreWindow^ window = Windows::UI::Core::CoreWindow::GetForCurrentThread();
	if (window != nullptr)
	{
		ff::ComPtr<ThreadDispatch, ff::IThreadDispatch> keepAlive = this;
		_handler = ref new Windows::UI::Core::DispatchedHandler([keepAlive]()
			{
				keepAlive->Flush(true);
			});

		_dispatcher = window->Dispatcher;
	}
#else
	static ff::StaticString s_name(L"ff::ThreadDispatchWindow");
	_window.SetListener(this);
	_window.CreateMessageWindow(s_name);
#endif

	return true;
}

void ThreadDispatch::Post(std::function<void()>&& func, bool runIfCurrentThread)
{
	InternalPost(std::move(func), runIfCurrentThread);
}

void ThreadDispatch::Send(std::function<void()>&& func)
{
	IThreadDispatch* dispatch = InternalPost(std::move(func), true);
	if (dispatch)
	{
		dispatch->Flush();
	}
}

ff::IThreadDispatch* ThreadDispatch::InternalPost(std::function<void()>&& func, bool runIfCurrentThread)
{
	assertRetVal(func != nullptr, nullptr);

	if (!_threadId)
	{
		// This could happen if a thread pool work item finishes after its
		// owner thread is gone. So just post somewhere else.
		IThreadDispatch* otherDispatch = ff::GetThreadDispatch();
		if (otherDispatch)
		{
			otherDispatch->Post(std::move(func), runIfCurrentThread);
		}
		else
		{
			// nowhere to post, so just run the function now
			func();
		}

		return otherDispatch;
	}

	if (runIfCurrentThread && IsCurrentThread())
	{
		func();
		return nullptr;
	}

	ff::LockMutex lock(_mutex);
	_funcs.Push(std::move(func));

	if (_funcs.Size() == 1)
	{
		::ResetEvent(_flushedEvent);

		if (!ff::IsEventSet(_pendingEvent))
		{
			::SetEvent(_pendingEvent);
			PostFlush();
		}
	}

	return this;
}

bool ThreadDispatch::IsCurrentThread() const
{
	return this != nullptr && _threadId == ::GetCurrentThreadId();
}

DWORD ThreadDispatch::GetThreadId() const
{
	return _threadId;
}

void ThreadDispatch::Flush()
{
	Flush(false);
}

void ThreadDispatch::Destroy()
{
	assertRet(IsCurrentThread());

	// Don't allow new work to be posted
	_threadId = 0;

	Flush(true);
}

size_t ThreadDispatch::WaitForAnyHandle(const HANDLE* handles, size_t count)
{
	assertRetVal(IsCurrentThread(), ff::INVALID_SIZE);
	assertRetVal(count < MAXIMUM_WAIT_OBJECTS - 1, ff::INVALID_SIZE);

	ff::Vector<HANDLE, 16> myHandles;
	myHandles.Reserve(count + 1);

	myHandles.Push(handles, count);
	myHandles.Push(_pendingEvent);

	while (true)
	{
#if METRO_APP
		DWORD result = ::WaitForMultipleObjectsEx((DWORD)myHandles.Size(), myHandles.Data(), FALSE, INFINITE, TRUE);
#else
		DWORD result = ::MsgWaitForMultipleObjectsEx((DWORD)myHandles.Size(), myHandles.Data(),
			INFINITE, QS_ALLINPUT, MWMO_ALERTABLE | MWMO_INPUTAVAILABLE);
#endif
		switch (result)
		{
		default:
			if (result < count)
			{
				return result;
			}
			else if (result == count)
			{
				Flush(false);

				if (!count)
				{
					return ff::INVALID_SIZE;
				}
			}
			else if (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + myHandles.Size())
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

#if !METRO_APP
		ff::HandleMessages();
#endif
	}
}

bool ThreadDispatch::WaitForAllHandles(const HANDLE* handles, size_t count)
{
	ff::Vector<HANDLE, 32> handleVector;
	handleVector.Push(handles, count);

	while (handleVector.Size())
	{
		size_t i = WaitForAnyHandle(handleVector.ConstData(), handleVector.Size());
		assertRetVal(i != ff::INVALID_SIZE, false);

		handleVector.Delete(i);
	}

	return true;
}

void ThreadDispatch::Flush(bool force)
{
	if (force || IsCurrentThread())
	{
		ff::Vector<Entry, 32> funcs;
		{
			ff::LockMutex lock(_mutex);
			funcs = std::move(_funcs);
		}

		for (const Entry& entry : funcs)
		{
			entry._func();
		}

		ff::LockMutex lock(_mutex);
		if (_funcs.IsEmpty())
		{
			::SetEvent(_flushedEvent);
			::ResetEvent(_pendingEvent);
		}
		else
		{
			// More stuff needs to run
			PostFlush();
		}
	}
	else
	{
		ff::WaitForHandle(_flushedEvent);
	}
}

void ThreadDispatch::PostFlush()
{
#if METRO_APP
	if (_dispatcher)
	{
		_dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, _handler);
	}
#else
	::PostMessage(_window.Handle(), WM_USER, 0, 0);
#endif
}

#if !METRO_APP
bool ThreadDispatch::ListenWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& nResult)
{
	switch (msg)
	{
	case WM_USER:
	case WM_DESTROY:
		Flush(true);
		break;
	}

	return false;
}
#endif

#endif
