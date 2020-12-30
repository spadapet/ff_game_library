#include "pch.h"
#include "file_o.h"
#include "resource_load_context.h"

ff::file_o::file_o(std::shared_ptr<ff::saved_data_base> saved_data, std::string_view file_extension, bool compress)
    : saved_data_(saved_data)
    , file_extension(file_extension)
    , compress(compress)
{
    assert(this->saved_data_);
}

const std::shared_ptr<ff::saved_data_base>& ff::file_o::saved_data() const
{
    return this->saved_data_;
}

bool ff::file_o::resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const
{
    std::filesystem::path path = (directory_path / name).replace_extension(this->file_extension);
    size_t size = this->saved_data_->loaded_size();
    size_t copied_size = ff::stream_copy(ff::file_writer(path), *this->saved_data_->loaded_reader(), size);
    return copied_size == size;
}

bool ff::file_o::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    allow_compress = this->compress;

    dict.set<ff::saved_data_base>("data", this->saved_data_);
    dict.set<std::string>("extension", this->file_extension);
    dict.set<bool>("compress", this->compress);

    return true;
}

std::shared_ptr<ff::resource_object_base> ff::file_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    std::filesystem::path path = dict.get<std::string>("file");
    if (path.empty())
    {
        context.add_error("Missing 'file' value");
        return nullptr;
    }

    std::error_code ec;
    uintmax_t max_size = std::filesystem::file_size(path, ec);
    if (max_size == static_cast<std::uintmax_t>(-1))
    {
        std::ostringstream str;
        str << "Failed to get size of file: " << path;
        context.add_error(str.str());
        return nullptr;
    }

    size_t size = static_cast<size_t>(max_size);
    auto saved_data = std::make_shared<ff::saved_data_file>(path, 0, size, size, ff::saved_data_type::none);

    bool default_compress = true;
    std::string file_extension = path.extension().string();

    if (file_extension == ".mp3" ||
        file_extension == ".png" ||
        file_extension == ".jpg")
    {
        default_compress = false;
    }

    bool compress = dict.get<bool>("compress", default_compress);
    return std::make_shared<file_o>(saved_data, file_extension, compress);
}

std::shared_ptr<ff::resource_object_base> ff::file_factory::load_from_cache(const ff::dict& dict) const
{
    auto saved_data = dict.get<ff::saved_data_base>("data");
    std::string file_extension = dict.get<std::string>("extension");
    bool compress = dict.get<bool>("compress", true);

    return saved_data ? std::make_shared<file_o>(saved_data, file_extension, compress) : nullptr;
}
