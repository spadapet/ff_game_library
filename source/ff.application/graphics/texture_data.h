#pragma once

namespace ff::internal
{
    std::shared_ptr<DirectX::ScratchImage> load_texture_data(const ff::resource_file& resource_file, DXGI_FORMAT new_format, size_t new_mip_count, std::shared_ptr<DirectX::ScratchImage>& palette);
}
