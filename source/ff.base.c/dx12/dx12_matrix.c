#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_matrix.h"

static_assert(sizeof(ff_matrix) == 64, "ff_matrix size changed");

static const ff_matrix s_rotate_matrices[ff_dx12_rotation_count][2] =
{
    {
        { .m = { 2, 0, 0, 0, 0, -2, 0, 0, 0, 0, 1, 0, -1, 1, 0, 1 } },
        { .m = { 2, 0, 0, 0, 0, -2, 0, 0, 0, 0, 1, 0, -1, 1, 0, 1 } },
    },
    {
        { .m = { 0, 2, 0, 0, 2, 0, 0, 0, 0, 0, 1, 0, -1, -1, 0, 1 } },
        { .m = { 2, 0, 0, 0, 0, -2, 0, 0, 0, 0, 1, 0, -1, 1, 0, 1 } },
    },
    {
        { .m = { -2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 1, -1, 0, 1 } },
        { .m = { 2, 0, 0, 0, 0, -2, 0, 0, 0, 0, 1, 0, -1, 1, 0, 1 } },
    },
    {
        { .m = { 0, -2, 0, 0, -2, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1 } },
        { .m = { 2, 0, 0, 0, 0, -2, 0, 0, 0, 0, 1, 0, -1, 1, 0, 1 } },
    },
};

ff_dx12_target_size ff_dx12_target_size_make(size_t width, size_t height)
{
    ff_dx12_target_size size = { 0 };
    size.pixel_size = ff_point_size_make(width, height);
    size.rotation = ff_dx12_rotation_none;
    size.dpi_scale = 1.0;
    return size;
}

ff_matrix ff_matrix_identity(void)
{
    ff_matrix matrix = { 0 };
    matrix.m[0] = 1.0f;
    matrix.m[5] = 1.0f;
    matrix.m[10] = 1.0f;
    matrix.m[15] = 1.0f;
    return matrix;
}

ff_matrix ff_matrix_multiply(ff_matrix l, ff_matrix r)
{
    ff_matrix matrix = { 0 };

    for (size_t row = 0; row < 4; row++)
    {
        for (size_t column = 0; column < 4; column++)
        {
            float sum = 0.0f;

            for (size_t i = 0; i < 4; i++)
            {
                sum += l.m[row * 4 + i] * r.m[i * 4 + column];
            }

            matrix.m[row * 4 + column] = sum;
        }
    }

    return matrix;
}

ff_matrix ff_matrix_transpose(ff_matrix value)
{
    ff_matrix matrix = { 0 };

    for (size_t row = 0; row < 4; row++)
    {
        for (size_t column = 0; column < 4; column++)
        {
            matrix.m[row * 4 + column] = value.m[column * 4 + row];
        }
    }

    return matrix;
}

ff_matrix ff_matrix_translation(float x, float y, float z)
{
    ff_matrix matrix = ff_matrix_identity();
    matrix.m[12] = x;
    matrix.m[13] = y;
    matrix.m[14] = z;
    return matrix;
}

ff_matrix ff_matrix_scaling(float x, float y, float z)
{
    ff_matrix matrix = { 0 };
    matrix.m[0] = x;
    matrix.m[5] = y;
    matrix.m[10] = z;
    matrix.m[15] = 1.0f;
    return matrix;
}

bool ff_matrix_equal(ff_matrix l, ff_matrix r)
{
    for (size_t i = 0; i < 16; i++)
    {
        FF_CHECK_RET_VAL(l.m[i] == r.m[i], false);
    }

    return true;
}

ff_point_float ff_matrix_transform_point(ff_matrix value, ff_point_float point)
{
    float x = point.x * value.m[0] + point.y * value.m[4] + value.m[12];
    float y = point.x * value.m[1] + point.y * value.m[5] + value.m[13];
    return ff_point_float_make(x, y);
}

bool ff_dx12_view_matrix(ff_dx12_target_size target_size, ff_rect_float view_rect, ff_rect_float world_rect, bool ignore_rotation, ff_matrix* matrix)
{
    FF_ASSERT_RET_VAL(matrix, false);
    *matrix = ff_matrix_identity();

    FF_ASSERT_RET_VAL(target_size.rotation >= 0 && target_size.rotation < ff_dx12_rotation_count, false);
    FF_CHECK_RET_VAL(ff_rect_float_area(world_rect) != 0.0f, false);
    FF_CHECK_RET_VAL(ff_rect_float_area(view_rect) > 0.0f, false);

    ff_matrix translate = ff_matrix_translation(-world_rect.left, -world_rect.top, 0.0f);
    ff_matrix scale = ff_matrix_scaling(1.0f / ff_rect_float_width(world_rect), 1.0f / ff_rect_float_height(world_rect), 1.0f);
    ff_matrix rotate = s_rotate_matrices[target_size.rotation][ignore_rotation ? 1 : 0];

    *matrix = ff_matrix_multiply(ff_matrix_multiply(translate, scale), rotate);
    return true;
}
