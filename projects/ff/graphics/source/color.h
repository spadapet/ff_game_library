#pragma once

namespace ff::color
{
    const DirectX::XMFLOAT4& none();
    const DirectX::XMFLOAT4& white();
    const DirectX::XMFLOAT4& black();
    const DirectX::XMFLOAT4& red();
    const DirectX::XMFLOAT4& green();
    const DirectX::XMFLOAT4& blue();
    const DirectX::XMFLOAT4& yellow();
    const DirectX::XMFLOAT4& cyan();
    const DirectX::XMFLOAT4& magenta();

}

namespace ff
{
    DirectX::XMFLOAT4 palette_index_to_color(int index);
    void palette_index_to_color(int index, DirectX::XMFLOAT4& color);
    void palette_index_to_color(const int* index, DirectX::XMFLOAT4* color, size_t count);
}
