#pragma once

typedef struct ff_dx12_descriptor_buffer ff_dx12_descriptor_buffer;

// descriptor_range is a value type: no destructor runs automatically. Callers must call
// ff_dx12_descriptor_range_free explicitly once the GPU is done with the descriptors.
typedef struct ff_dx12_descriptor_range
{
    ff_dx12_descriptor_buffer* owner;
    size_t start;
    size_t count;
} ff_dx12_descriptor_range;

bool ff_dx12_descriptor_range_valid(const ff_dx12_descriptor_range* range);
D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_range_cpu_handle(const ff_dx12_descriptor_range* range, size_t index);
D3D12_GPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_range_gpu_handle(const ff_dx12_descriptor_range* range, size_t index);

// Index of a descriptor within its owning heap, which is the value a shader uses to index
// ResourceDescriptorHeap[] under bindless. Returns SIZE_MAX for an invalid range or index.
size_t ff_dx12_descriptor_range_heap_index(const ff_dx12_descriptor_range* range, size_t index);

// Explicit free, replacing the old C++ destructor. Safe to call on an already-freed or
// zero-initialized range.
void ff_dx12_descriptor_range_free(ff_dx12_descriptor_range* range);
