#pragma once

typedef struct ff_dx12_descriptor_buffer ff_dx12_descriptor_buffer;

typedef struct ff_dx12_descriptor_range
{
    ff_dx12_descriptor_buffer* owner;
    size_t start;
    size_t count;
} ff_dx12_descriptor_range;

bool ff_dx12_descriptor_range_valid(const ff_dx12_descriptor_range* range);
void ff_dx12_descriptor_range_free(ff_dx12_descriptor_range* range);

D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_range_cpu_handle(const ff_dx12_descriptor_range* range, size_t index);
D3D12_GPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_range_gpu_handle(const ff_dx12_descriptor_range* range, size_t index);
