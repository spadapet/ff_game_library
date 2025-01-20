#pragma once

namespace ff
{
    class color
    {
    public:
        struct palette_t
        {
            bool operator==(const ff::color::palette_t& other) const = default;
            bool operator!=(const ff::color::palette_t& other) const = default;

            int index;
            float alpha;
        };

        color() = default;
        color(const ff::color& other) = default;
        color(float r, float g, float b, float a = 1.0f);
        color(const DirectX::XMFLOAT4& rgba);
        color(int index, float alpha = 1.0f);
        color(const ff::color::palette_t& palette);

        ff::color& operator=(const ff::color& other) = default;
        ff::color& operator=(const DirectX::XMFLOAT4& other);
        ff::color& operator=(const ff::color::palette_t& other);

        bool operator==(const ff::color& other) const;
        bool operator==(const DirectX::XMFLOAT4& other) const;
        bool operator==(const ff::color::palette_t& other) const;

        bool operator!=(const ff::color& other) const = default;
        bool operator!=(const DirectX::XMFLOAT4& other) const;
        bool operator!=(const ff::color::palette_t& other) const;

        bool is_rgba() const;
        bool is_palette() const;

        const DirectX::XMFLOAT4& rgba() const;
        const ff::color::palette_t& palette() const;
        static const ff::color& cast(const DirectX::XMFLOAT4& other);
        static const ff::color* cast(const DirectX::XMFLOAT4* other);

        operator const DirectX::XMFLOAT4& () const;
        operator const ff::color::palette_t& () const;

        DirectX::XMFLOAT4 to_shader_color(const uint8_t* index_remap = nullptr) const;
        void to_shader_color(DirectX::XMFLOAT4& color, const uint8_t* index_remap = nullptr) const;

    private:
        static constexpr int type_palette = -1;

        struct type_t
        {
            int type;
            ff::color::palette_t palette;
        };

        union data_t
        {
            DirectX::XMFLOAT4 rgba{};
            ff::color::type_t palette;
        } data;
    };

    const ff::color& color_none();
    const ff::color& color_none_palette();
    const ff::color& color_white();
    const ff::color& color_black();
    const ff::color& color_red();
    const ff::color& color_green();
    const ff::color& color_blue();
    const ff::color& color_yellow();
    const ff::color& color_cyan();
    const ff::color& color_magenta();
}
