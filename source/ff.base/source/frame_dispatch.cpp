#include "pch.h"
#include "frame_dispatch.h"

static thread_local ff::frame_dispatch* current_frame_dispatch = nullptr;

ff::frame_dispatch::frame_dispatch()
    : thread_id(::GetCurrentThreadId())
{
}

ff::frame_dispatch::~frame_dispatch()
{
    this->destroyed = true;
    this->flush();
}

void ff::frame_dispatch::post(std::function<void()>&& func, bool run_if_current_thread)
{
}

void ff::frame_dispatch::flush()
{
}

ff::frame_dispatch::scope::scope(frame_dispatch& dispatch)
    : old_dispatch(::current_frame_dispatch)
{
    ::current_frame_dispatch = &dispatch;
}

ff::frame_dispatch::scope::~scope()
{
    ::current_frame_dispatch->flush();
    ::current_frame_dispatch = this->old_dispatch;
}
