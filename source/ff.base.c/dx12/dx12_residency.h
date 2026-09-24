#pragma once

#include "dx12_fence_values.h"

typedef enum ff_dx12_residency_owner_type
{
    ff_dx12_residency_owner_type_heap,
} ff_dx12_residency_owner_type;

// Intrusive MRU/LRU list node wrapping one ID3D12Pageable. Single-threaded v1: the old code
// took a mutex around list mutation and around make_resident's list walk; there is none here.
typedef struct ff_dx12_residency_data
{
    struct ff_dx12_residency_data* prev;
    struct ff_dx12_residency_data* next;
    wchar_t name[64];
    ID3D12Pageable* pageable;
    uint64_t size;
    bool resident;
    ff_dx12_fence_value resident_value;
    ff_dx12_fence_values keep_resident;
    uint32_t usage_counter;
} ff_dx12_residency_data;

// pageable is not ref-counted here; the owner (for example ff_dx12_heap) must keep it alive for
// at least as long as this residency_data.
// arena backs the keep_resident set, which grows with the number of executes referencing this
// pageable. It must outlive the residency_data.
void ff_dx12_residency_data_init(ff_dx12_residency_data* data, ff_arena* arena, ff_string_view name, ID3D12Pageable* pageable, uint64_t size, bool resident);
void ff_dx12_residency_data_destroy(ff_dx12_residency_data* data);

// Global lifecycle for the residency subsystem (the singleton fence used to signal
// make-resident completion). ff_dx12_init/ff_dx12_destroy own this lifecycle.
bool ff_dx12_residency_init(void);
void ff_dx12_residency_destroy(void);

bool ff_dx12_make_resident(ff_dx12_residency_data** residency_set, size_t residency_set_count,
    ff_dx12_fence_value commands_fence_value, ff_dx12_fence_values* wait_values);
