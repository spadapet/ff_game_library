#include "pch.h"
#include "thread_pool.h"

#if 0

#include "COM/ComAlloc.h"
#include "COM/ComObject.h"
#include "Globals/ProcessGlobals.h"
#include "Thread/ThreadDispatch.h"
#include "Thread/ThreadPool.h"
#include "Thread/ThreadUtil.h"
#include "Windows/Handles.h"

class __declspec(uuid("58c5d38c-72f2-4b50-a92b-c92f14682f9c"))
	ThreadPool : public ff::ComBase, public ff::IThreadPool
{
public:
	DECLARE_HEADER(ThreadPool);

	// IThreadPool functions
	virtual void AddThread(std::function<void()>&& runFunc) override;
	virtual void AddTask(std::function<void()>&& runFunc, std::function<void()>&& completeFunc) override;
	virtual void FlushTasks() override;
	virtual void Destroy() override;

private:
	struct ThreadEntry
	{
		std::function<void()> _runFunc;
	};

	struct TaskEntry
	{
		std::function<void()> _runFunc;
		std::function<void()> _completeFunc;
		ff::ComPtr<ff::IThreadDispatch> _dispatch;
		ff::ComPtr<ThreadPool, ff::IThreadPool> _threadPool;
	};

	static void CALLBACK RunThreadCallback(PTP_CALLBACK_INSTANCE instance, void* context);
	static void CALLBACK RunTaskCallback(PTP_CALLBACK_INSTANCE instance, void* context);
	void RunTask(TaskEntry* task);

	ff::Mutex _mutex;
	ff::WinHandle _eventNoTasks;
	ff::PoolAllocator<TaskEntry, false> _taskAllocator;
	size_t _taskCount;
	bool _destroyed;
};

BEGIN_INTERFACES(ThreadPool)
	HAS_INTERFACE(ff::IThreadPool)
END_INTERFACES()

ff::ComPtr<ff::IThreadPool> ff::CreateThreadPool()
{
	ff::ComPtr<ThreadPool> myObj;
	assertHrRetVal(ff::ComAllocator<ThreadPool>::CreateInstance(&myObj), false);
	return myObj.Interface();
}

ff::IThreadPool* ff::GetThreadPool()
{
	return ff::ProcessGlobals::Exists()
		? ff::ProcessGlobals::Get()->GetThreadPool()
		: nullptr;
}

ThreadPool::ThreadPool()
	: _taskCount(0)
	, _destroyed(false)
{
	_eventNoTasks = ff::CreateEvent(true);
}

ThreadPool::~ThreadPool()
{
	assertSz(!_taskCount && _destroyed, L"The owner of ThreadPool should've called Destroy() by now");
}

void ThreadPool::AddThread(std::function<void()>&& runFunc)
{
	ThreadEntry* entry = new ThreadEntry{ std::move(runFunc) };
	verify(::TrySubmitThreadpoolCallback(ThreadPool::RunThreadCallback, entry, nullptr));
}

void ThreadPool::AddTask(std::function<void()>&& runFunc, std::function<void()>&& completeFunc)
{
	ff::IThreadDispatch* dispatch = ff::GetThreadDispatch();
	TaskEntry* task = nullptr;
	bool runTaskNow = false;
	{
		ff::LockMutex lock(_mutex);

		_taskCount++;
		task = _taskAllocator.New();
		task->_runFunc = std::move(runFunc);
		task->_completeFunc = std::move(completeFunc);
		task->_dispatch = dispatch;
		task->_threadPool = this;

		if (!_destroyed && dispatch)
		{
			verify(::TrySubmitThreadpoolCallback(ThreadPool::RunTaskCallback, task, nullptr));
			::ResetEvent(_eventNoTasks);
		}
		else
		{
			runTaskNow = true;
		}
	}

	if (runTaskNow)
	{
		RunTask(task);
	}
}

void ThreadPool::FlushTasks()
{
	ff::WaitForHandle(_eventNoTasks);
}

void ThreadPool::Destroy()
{
	// Don't let new tasks get added
	{
		ff::LockMutex lock(_mutex);
		_destroyed = true;
	}

	FlushTasks();
}

void ThreadPool::RunThreadCallback(PTP_CALLBACK_INSTANCE instance, void* context)
{
	ThreadEntry* entry = (ThreadEntry*)context;
	context = nullptr;

	::CallbackMayRunLong(instance);
	::DisassociateCurrentThreadFromCallback(instance);
	::SetThreadDescription(::GetCurrentThread(), L"ff : Background Thread");

	std::function<void()> runFunc(std::move(entry->_runFunc));
	delete entry;
	entry = nullptr;

	runFunc();
}

void ThreadPool::RunTaskCallback(PTP_CALLBACK_INSTANCE instance, void* context)
{
#if _DEBUG
	::SetThreadDescription(::GetCurrentThread(), L"ff : Background Task");
#endif
	TaskEntry* task = (TaskEntry*)context;
	task->_threadPool->RunTask(task);
	::DisassociateCurrentThreadFromCallback(instance);
}

void ThreadPool::RunTask(TaskEntry* task)
{
	task->_runFunc();

	task->_dispatch->Post([task]()
		{
			std::function<void()> completeFunc = std::move(task->_completeFunc);
			{
				ff::ComPtr<ThreadPool, ff::IThreadPool> threadPool = std::move(task->_threadPool);
				ff::LockMutex lock(threadPool->_mutex);
				threadPool->_taskAllocator.Delete(task);

				if (!--threadPool->_taskCount)
				{
					::SetEvent(threadPool->_eventNoTasks);
				}
			}

			if (completeFunc)
			{
				completeFunc();
			}
		});
}

#endif
