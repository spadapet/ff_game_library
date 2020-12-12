#pragma once

namespace ff
{
    enum class thread_type
    {
        none,
        main,
        game,
    };

    class thread_dispach
    {
    public:
        thread_dispach(thread_type type);
        ~thread_dispach();

        static thread_dispach* get(thread_type type = thread_type::none);

        virtual void post(std::function<void()>&& func, bool runIfCurrentThread = false) = 0;
        virtual void send(std::function<void()>&& func) = 0;
        virtual bool current_thread() const = 0;
        virtual uint32_t thread_id() const = 0;

        virtual void Flush() = 0;
        virtual void Destroy() = 0;

        virtual size_t WaitForAnyHandle(const HANDLE* handles, size_t count) = 0;
        virtual bool WaitForAllHandles(const HANDLE* handles, size_t count) = 0;

    private:
        IThreadDispatch* InternalPost(std::function<void()>&& func, bool runIfCurrentThread);
        void Flush(bool force);
        void PostFlush();

        struct Entry
        {
            Entry()
            {		}

            Entry(std::function<void()>&& func)
                : _func(std::move(func))
            {		}

            std::function<void()> _func;
        };

        std::recursive_mutex mutex;
        thread_type type;
        uint3_t thread_id_data;
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
}

#if 0

namespace ff
{
	class __declspec(uuid("f42db5ff-728e-4717-9792-b6733e875021")) __declspec(novtable)
		IThreadDispatch : public IUnknown
	{
	public:
		virtual void Post(std::function<void()>&& func, bool runIfCurrentThread = false) = 0;
		virtual void Send(std::function<void()>&& func) = 0;
		virtual bool IsCurrentThread() const = 0;
		virtual DWORD GetThreadId() const = 0;

		virtual void Flush() = 0;
		virtual void Destroy() = 0;

		virtual size_t WaitForAnyHandle(const HANDLE* handles, size_t count) = 0;
		virtual bool WaitForAllHandles(const HANDLE* handles, size_t count) = 0;
	};

	UTIL_API ComPtr<IThreadDispatch> CreateCurrentThreadDispatch();
	UTIL_API IThreadDispatch* GetThreadDispatch();
	UTIL_API IThreadDispatch* GetCurrentThreadDispatch();
	UTIL_API IThreadDispatch* GetMainThreadDispatch();
	UTIL_API IThreadDispatch* GetGameThreadDispatch();
}

#endif
