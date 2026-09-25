#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_texture.h"
#include "dx12/dx12_texture_view.h"

bool ff_dx12_texture_view_init(ff_dx12_texture_view* view, ff_dx12_texture* texture,
    size_t array_start, size_t array_count, size_t mip_start, size_t mip_count)
{
    FF_ASSERT_RET_VAL(view, false);

    *view = (ff_dx12_texture_view){ 0 };

    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture), false);

    const size_t texture_array_size = ff_dx12_texture_array_size(texture);
    const size_t texture_mip_count = ff_dx12_texture_mip_count(texture);

    FF_ASSERT_RET_VAL(array_start < texture_array_size && mip_start < texture_mip_count, false);

    if (!array_count)
    {
        array_count = texture_array_size - array_start;
    }

    if (!mip_count)
    {
        mip_count = texture_mip_count - mip_start;
    }

    FF_ASSERT_RET_VAL(array_count <= texture_array_size - array_start, false);
    FF_ASSERT_RET_VAL(mip_count <= texture_mip_count - mip_start, false);

    view->texture = texture;
    view->array_start = array_start;
    view->array_count = array_count;
    view->mip_start = mip_start;
    view->mip_count = mip_count;

    ff_dx12_add_device_child(&view->device_child, view, ff_dx12_device_child_type_texture_view);
    return true;
}

void ff_dx12_texture_view_destroy(ff_dx12_texture_view* view)
{
    FF_CHECK_RET(view);

    ff_dx12_remove_device_child(&view->device_child);
    ff_dx12_descriptor_range_free(&view->view);

    *view = (ff_dx12_texture_view){ 0 };
}

bool ff_dx12_texture_view_valid(const ff_dx12_texture_view* view)
{
    return view && view->texture && ff_dx12_texture_valid(view->texture);
}

ff_dx12_texture* ff_dx12_texture_view_texture(ff_dx12_texture_view* view)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_view_valid(view), NULL);
    return view->texture;
}

size_t ff_dx12_texture_view_array_start(const ff_dx12_texture_view* view)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_view_valid(view), 0);
    return view->array_start;
}

size_t ff_dx12_texture_view_array_count(const ff_dx12_texture_view* view)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_view_valid(view), 0);
    return view->array_count;
}

size_t ff_dx12_texture_view_mip_start(const ff_dx12_texture_view* view)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_view_valid(view), 0);
    return view->mip_start;
}

size_t ff_dx12_texture_view_mip_count(const ff_dx12_texture_view* view)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_view_valid(view), 0);
    return view->mip_count;
}

D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_texture_view_cpu_handle(ff_dx12_texture_view* view)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_view_valid(view), (D3D12_CPU_DESCRIPTOR_HANDLE){ 0 });

    if (!ff_dx12_descriptor_range_valid(&view->view))
    {
        view->view = ff_dx12_cpu_descriptor_allocator_alloc(ff_dx12_cpu_buffer_descriptors(), 1);
        FF_ASSERT_RET_VAL(ff_dx12_descriptor_range_valid(&view->view), (D3D12_CPU_DESCRIPTOR_HANDLE){ 0 });

        ff_dx12_resource_create_shader_view(ff_dx12_texture_resource(view->texture),
            ff_dx12_descriptor_range_cpu_handle(&view->view, 0),
            view->array_start, view->array_count, view->mip_start, view->mip_count);
    }

    return ff_dx12_descriptor_range_cpu_handle(&view->view, 0);
}

bool internal_ff_dx12_texture_view_reset(ff_dx12_texture_view* view)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_view_valid(view), false);

    if (ff_dx12_descriptor_range_valid(&view->view))
    {
        ff_dx12_resource_create_shader_view(ff_dx12_texture_resource(view->texture),
            ff_dx12_descriptor_range_cpu_handle(&view->view, 0),
            view->array_start, view->array_count, view->mip_start, view->mip_count);

        view->reset_count++;
    }

    return true;
}

size_t ff_dx12_texture_view_reset_count(const ff_dx12_texture_view* view)
{
    FF_ASSERT_RET_VAL(view, 0);
    return view->reset_count;
}
