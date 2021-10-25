#pragma once

namespace ff::dxgi
{
    const DirectX::XMFLOAT4& color_none();
    const DirectX::XMFLOAT4& color_white();
    const DirectX::XMFLOAT4& color_black();
    const DirectX::XMFLOAT4& color_red();
    const DirectX::XMFLOAT4& color_green();
    const DirectX::XMFLOAT4& color_blue();
    const DirectX::XMFLOAT4& color_yellow();
    const DirectX::XMFLOAT4& color_cyan();
    const DirectX::XMFLOAT4& color_magenta();

    DirectX::XMFLOAT4 palette_index_to_color(int index, float alpha = 1.0f);
    void palette_index_to_color(int index, DirectX::XMFLOAT4& color, float alpha = 1.0f);
    void palette_index_to_color(const int* index, DirectX::XMFLOAT4* color, size_t count, float alpha = 1.0f);
}
