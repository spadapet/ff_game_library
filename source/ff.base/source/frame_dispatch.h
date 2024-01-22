#pragma once

namespace ff
{
    class frame_dispatch
    {
    public:
        frame_dispatch();
        frame_dispatch(frame_dispatch&& other) noexcept = delete;
        frame_dispatch(const frame_dispatch& other) = delete;
        ~frame_dispatch();

        frame_dispatch& operator=(frame_dispatch&& other) noexcept = delete;
        frame_dispatch& operator=(const frame_dispatch& other) = delete;

        static frame_dispatch* get();

        void post(std::function<void()>&& func, bool run_if_current_thread = false);
        void flush();

        class scope
        {
        public:
            scope(frame_dispatch& dispatch);
            ~scope();

        private:
            ff::frame_dispatch* old_dispatch;
        };

    private:
        std::forward_list<std::function<void()>> funcs;
        DWORD thread_id;
        bool destroyed{};
    };
}
