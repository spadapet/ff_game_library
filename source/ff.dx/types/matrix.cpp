#include "pch.h"
#include "types/matrix.h"

static const DirectX::XMFLOAT4X4 identity_matrix_4x4
(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
);

static const DirectX::XMFLOAT3X3 identity_matrix_3x3
(
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
);

const DirectX::XMFLOAT4X4& ff::matrix_identity_4x4()
{
    return ::identity_matrix_4x4;
}

const DirectX::XMFLOAT3X3& ff::matrix_identity_3x3()
{
    return ::identity_matrix_3x3;
}
