#pragma once

#include "../base/arena.h"
#include "dx12_fence.h"
#include "dx12_fence_values.h"
#include "dx12_residency.h"
#include "dx12_resource_tracker.h"

typedef struct ff_dx12_commands ff_dx12_commands;
typedef struct ff_dx12_queue ff_dx12_queue;

// Residency data pointers gathered while recording one command list. The old code used an
// unordered_set; this is an open-addressed set so that recording a resource many times in one
// list doesn't push duplicate entries at the residency manager. It grows out of the owning
// queue's arena rather than capping the number of resources a single list can touch.
#define FF_DX12_RESIDENCY_SET_MIN 256

typedef struct ff_dx12_residency_set
{
    ff_dx12_residency_data** slots;
    size_t capacity;
    size_t count;
} ff_dx12_residency_set;

void ff_dx12_residency_set_clear(ff_dx12_residency_set* set);
bool ff_dx12_residency_set_add(ff_arena* arena, ff_dx12_residency_set* set, ff_dx12_residency_data* data);

// The per-command-list state that outlives an ff_dx12_commands and gets recycled by the queue.
// Nodes are arena-allocated and never freed individually, so their addresses are stable and an
// ff_dx12_commands can point at one.
typedef struct ff_dx12_command_cache
{
    struct ff_dx12_command_cache* next;

    ID3D12GraphicsCommandList1* list;
    ID3D12GraphicsCommandList* list_before;
    ID3D12CommandAllocator* allocator;
    ID3D12CommandAllocator* allocator_before;

    ff_dx12_residency_set residency_set;
    ff_dx12_resource_tracker resource_tracker;
    ff_dx12_fence fence;

    // Set while the cache is sitting in the queue's recycle list with its lists already closed.
    // A cache can only be handed back out after its lists have been reset.
    bool needs_reset;
} ff_dx12_command_cache;

// An allocator waiting for the GPU to finish with it, kept in FIFO order so the front is always
// the oldest. Nodes are recycled through the queue's free list, never freed individually.
typedef struct ff_dx12_queue_allocator_node
{
    struct ff_dx12_queue_allocator_node* next;
    ID3D12CommandAllocator* allocator;
    ff_dx12_fence_value fence_value;
} ff_dx12_queue_allocator_node;

typedef struct ff_dx12_queue_allocator_list
{
    ff_dx12_queue_allocator_node* head;
    ff_dx12_queue_allocator_node* tail;
} ff_dx12_queue_allocator_list;

typedef struct ff_dx12_queue
{
    ff_arena arena;
    ID3D12CommandQueue* command_queue;
    D3D12_COMMAND_LIST_TYPE type;
    wchar_t name[64];

    ff_dx12_fence idle_fence;

    ff_dx12_command_cache* caches;
    ff_dx12_queue_allocator_list allocators;
    ff_dx12_queue_allocator_list allocators_before;
    ff_dx12_queue_allocator_node* allocator_nodes_free;

    int list_counter;
    int allocator_counter;
} ff_dx12_queue;

bool ff_dx12_queue_init(ff_dx12_queue* queue, ff_string_view name, D3D12_COMMAND_LIST_TYPE type);
void ff_dx12_queue_destroy(ff_dx12_queue* queue);

bool ff_dx12_queue_valid(const ff_dx12_queue* queue);
ID3D12CommandQueue* ff_dx12_queue_command_queue(ff_dx12_queue* queue);
D3D12_COMMAND_LIST_TYPE ff_dx12_queue_type(const ff_dx12_queue* queue);
void ff_dx12_queue_wait_for_idle(ff_dx12_queue* queue);

// The returned commands points into queue-owned storage and must be passed to
// ff_dx12_queue_execute (or ff_dx12_commands_destroy) before the queue is destroyed.
bool ff_dx12_queue_new_commands(ff_dx12_queue* queue, ff_dx12_commands* commands);

ff_dx12_fence_value ff_dx12_queue_execute(ff_dx12_queue* queue, ff_dx12_commands* commands);
void ff_dx12_queue_execute_many(ff_dx12_queue* queue, ff_dx12_commands** commands, size_t count);
