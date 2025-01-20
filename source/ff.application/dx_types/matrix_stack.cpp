#include "pch.h"
#include "matrix.h"
#include "matrix_stack.h"
#include "operators.h"

ff::matrix_stack::matrix_stack()
    : stack{ ff::matrix_identity_4x4() }
{}

const DirectX::XMFLOAT4X4& ff::matrix_stack::matrix() const
{
    return this->stack[this->stack.size() - 1];
}

void ff::matrix_stack::push()
{
    DirectX::XMFLOAT4X4 matrix = this->matrix();
    this->stack.push_back(matrix);
}

void ff::matrix_stack::pop()
{
    if (this->stack.size() <= 1)
    {
        assert(false);
        return;
    }

    bool change = this->stack[this->stack.size() - 1] != this->stack[this->stack.size() - 2];
    if (change)
    {
        this->matrix_changing_.notify(*this);
    }

    this->stack.pop_back();

    if (change)
    {
        this->matrix_changed_.notify(*this);
    }
}

void ff::matrix_stack::reset(const DirectX::XMFLOAT4X4* default_matrix)
{
    this->stack.resize(1);

    if (default_matrix)
    {
        this->stack[0] = *default_matrix;
    }
}

void ff::matrix_stack::set(const DirectX::XMFLOAT4X4& matrix)
{
    if (matrix != this->matrix())
    {
        this->matrix_changing_.notify(*this);

        if (this->stack.size() == 1)
        {
            this->push();
        }

        this->stack[this->stack.size() - 1] = matrix;

        this->matrix_changed_.notify(*this);
    }
}

void ff::matrix_stack::transform(const DirectX::XMFLOAT4X4& matrix)
{
    if (matrix != ff::matrix_identity_4x4())
    {
        this->matrix_changing_.notify(*this);

        if (this->stack.size() == 1)
        {
            this->push();
        }

        DirectX::XMFLOAT4X4& my_matrix = this->stack[this->stack.size() - 1];
        DirectX::XMStoreFloat4x4(&my_matrix, DirectX::XMLoadFloat4x4(&matrix) * DirectX::XMLoadFloat4x4(&my_matrix));

        this->matrix_changed_.notify(*this);
    }
}

ff::signal_sink<const ff::matrix_stack&>& ff::matrix_stack::matrix_changing()
{
    return this->matrix_changing_;
}

ff::signal_sink<const ff::matrix_stack&>& ff::matrix_stack::matrix_changed()
{
    return this->matrix_changed_;
}
