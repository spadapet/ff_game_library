#pragma once

#include "../base/rect.h"

typedef enum ff_dx12_rotation
{
    ff_dx12_rotation_none, // DMDO_DEFAULT
    ff_dx12_rotation_90, // DMDO_90
    ff_dx12_rotation_180, // DMDO_180
    ff_dx12_rotation_270, // DMDO_270
    ff_dx12_rotation_count,
} ff_dx12_rotation;

typedef struct ff_dx12_target_size
{
    ff_point_size pixel_size;
    ff_dx12_rotation rotation;
    double dpi_scale;
} ff_dx12_target_size;

typedef struct ff_matrix
{
    float m[16]; // Row-vector convention
} ff_matrix;

ff_dx12_target_size ff_dx12_target_size_make(size_t width, size_t height);

ff_matrix ff_matrix_identity(void);
ff_matrix ff_matrix_multiply(const ff_matrix* l, const ff_matrix* r);
ff_matrix ff_matrix_transpose(const ff_matrix* value);
ff_matrix ff_matrix_translation(float x, float y, float z);
ff_matrix ff_matrix_scaling(float x, float y, float z);
bool ff_matrix_equal(const ff_matrix* l, const ff_matrix* r);

ff_point_float ff_matrix_transform_point(const ff_matrix* matrix, ff_point_float point);
bool ff_dx12_view_matrix(ff_dx12_target_size target_size, ff_rect_float view_rect, ff_rect_float world_rect, bool ignore_rotation, ff_matrix* matrix);
