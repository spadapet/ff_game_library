#include "pch.h"
#include "pageable.h"

static std::mutex pageable_mutex;
static ff::dx12::pageable_base* pageable_front;
static ff::dx12::pageable_base* pageable_back;

ff::dx12::pageable_base::pageable_base()
{
    std::scoped_lock lock(::pageable_mutex);
    ff::intrusive_list::add_back(::pageable_front, ::pageable_back, this);
}

ff::dx12::pageable_base::pageable_base(pageable_base&& other) noexcept
{
    std::scoped_lock lock(::pageable_mutex);
    ff::intrusive_list::add_back(::pageable_front, ::pageable_back, this);
}

ff::dx12::pageable_base::~pageable_base()
{
    std::scoped_lock lock(::pageable_mutex);
    ff::intrusive_list::remove(::pageable_front, ::pageable_back, this);
}

ff::dx12::pageable_base& ff::dx12::pageable_base::operator=(pageable_base&& other) noexcept
{
    return *this;
}

void ff::dx12::pageable_base::make_resident(const std::unordered_set<ff::dx12::pageable_base*>& pageable_set, ff::dx12::fence_values& wait_values)
{}
