#pragma once

namespace ff::dxgi
{
    class matrix_stack
    {
    public:
        matrix_stack();
        matrix_stack(matrix_stack&& other) noexcept = default;
        matrix_stack(const matrix_stack& other) = delete;

        matrix_stack& operator=(matrix_stack&& other) noexcept = default;
        matrix_stack& operator=(const matrix_stack & other) = delete;

        const DirectX::XMFLOAT4X4& matrix() const;
        void push();
        void pop();
        void reset(const DirectX::XMFLOAT4X4* default_matrix = nullptr); // only should be called by the owner

        void set(const DirectX::XMFLOAT4X4& matrix);
        void transform(const DirectX::XMFLOAT4X4& matrix);

        ff::signal_sink<const matrix_stack&>& matrix_changing();
        ff::signal_sink<const matrix_stack&>& matrix_changed();

    private:
        std::vector<DirectX::XMFLOAT4X4> stack;
        ff::signal<const matrix_stack&> matrix_changing_;
        ff::signal<const matrix_stack&> matrix_changed_;
    };
}
