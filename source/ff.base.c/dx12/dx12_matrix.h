#pragma once

#include "../base/rect.h"

// Values match the Win32 DEVMODE dmDisplayOrientation constants (DMDO_DEFAULT, DMDO_90,
// DMDO_180, DMDO_270), so a queried orientation can be assigned directly.
typedef enum ff_dx12_rotation
{
    ff_dx12_rotation_none,
    ff_dx12_rotation_90,
    ff_dx12_rotation_180,
    ff_dx12_rotation_270,
    ff_dx12_rotation_count,
} ff_dx12_rotation;

// Rotation and dpi_scale are always none and 1.0 today. They exist so the view matrix signature
// doesn't have to change when display rotation is implemented.
typedef struct ff_dx12_target_size
{
    ff_point_size pixel_size;
    ff_dx12_rotation rotation;
    double dpi_scale;
} ff_dx12_target_size;

// Row-vector convention: a point is transformed as v * m, and translation lives in row 3.
// This matches the legacy DirectXMath layout the rotation tables were authored in.
typedef struct ff_matrix
{
    float m[16];
} ff_matrix;

ff_dx12_target_size ff_dx12_target_size_make(size_t width, size_t height);

ff_matrix ff_matrix_identity(void);
ff_matrix ff_matrix_multiply(ff_matrix l, ff_matrix r);
ff_matrix ff_matrix_transpose(ff_matrix value);
ff_matrix ff_matrix_translation(float x, float y, float z);
ff_matrix ff_matrix_scaling(float x, float y, float z);
bool ff_matrix_equal(ff_matrix l, ff_matrix r);

ff_point_float ff_matrix_transform_point(ff_matrix value, ff_point_float point);

// Maps world_rect onto the target's clip space. Returns identity and false when either rect has
// no area, since the projection would divide by zero.
bool ff_dx12_view_matrix(ff_dx12_target_size target_size, ff_rect_float view_rect, ff_rect_float world_rect, bool ignore_rotation, ff_matrix* matrix);
