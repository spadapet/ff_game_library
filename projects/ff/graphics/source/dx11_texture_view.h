#pragma once

namespace ff
{
    class dx11_texture_o;

    class dx11_texture_view
    {
    public:
        dx11_texture_view(const std::shared_ptr<dx11_texture_o>& texture, size_t array_start, size_t array_count, size_t mip_start, size_t mip_count);
        dx11_texture_view(dx11_texture_view&& other) noexcept = default;
        dx11_texture_view(const dx11_texture_view& other) = delete;

        dx11_texture_view& operator=(dx11_texture_view&& other) noexcept = default;
        dx11_texture_view& operator=(const dx11_texture_view & other) = delete;

    private:
        std::shared_ptr<dx11_texture_o> texture_;
        size_t array_start_;
        size_t array_count_;
        size_t mip_start_;
        size_t mip_count_;
    };
}
