#include "pch.h"
#include "data_blob.h"
#include "shader.h"

namespace
{
    class shader_include : public ID3DInclude
    {
    public:
        shader_include(const std::filesystem::path& base_path)
            : base_path(base_path)
        {}

        virtual HRESULT __stdcall Open(D3D_INCLUDE_TYPE type, const char* file, const void* parent_data, const void** out_data, UINT* out_size) override
        {
            if (file && *file && out_data && out_size)
            {
                std::filesystem::path full_path = this->base_path / std::filesystem::path(std::string_view(file));
                ff::file_mem_mapped map_file(full_path);

                if (map_file)
                {
                    *out_data = new uint8_t[map_file.size()];
                    *out_size = static_cast<UINT>(map_file.size());
                    std::memcpy(const_cast<void*>(*out_data), map_file.data(), map_file.size());
                    return S_OK;
                }
            }

            return E_FAIL;
        }

        virtual HRESULT __stdcall Close(const void* data) override
        {
            delete[] reinterpret_cast<uint8_t*>(const_cast<void*>(data));
            return S_OK;
        }

    private:
        std::filesystem::path base_path;
    };
}

static std::shared_ptr<ff::data_base> compile_shader(
    const std::shared_ptr<ff::data_base>& file_data,
    const std::filesystem::path& file_path,
    const std::filesystem::path& base_path,
    std::string_view entry,
    std::string_view target,
    const std::unordered_map<std::string_view, std::string_view>& defines,
    bool debug,
    std::vector<std::string>& compile_errors)
{
    if (entry.empty() || target.empty() || !file_data)
    {
        return nullptr;
    }

    const char* level9 = "0";
    if (ff::string::ends_with(target, "_level_9_1"))
    {
        level9 = "1";
    }
    else if (ff::string::ends_with(target, "_level_9_2"))
    {
        level9 = "2";
    }
    else if (ff::string::ends_with(target, "_level_9_3"))
    {
        level9 = "3";
    }

    std::forward_list<std::string> strings; // need to ensure null terminated strings
    std::vector<D3D_SHADER_MACRO> defines_vector;
    for (auto& i : defines)
    {
        defines_vector.push_back(D3D_SHADER_MACRO{ strings.emplace_front(i.first).data(), strings.emplace_front(i.second).data() });
    }

    defines_vector.push_back(D3D_SHADER_MACRO{ "ENTRY", strings.emplace_front(entry).data() });
    defines_vector.push_back(D3D_SHADER_MACRO{ "TARGET", strings.emplace_front(target).data() });
    defines_vector.push_back(D3D_SHADER_MACRO{ "LEVEL9", level9 });
    defines_vector.push_back(D3D_SHADER_MACRO{ nullptr, nullptr });

    Microsoft::WRL::ComPtr<ID3DBlob> shader_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> compile_errors_blob;
    ::shader_include shader_include(base_path);

    HRESULT hr = ::D3DCompile(
        file_data->data(),
        file_data->size(),
        ff::filesystem::to_string(file_path).c_str(),
        defines_vector.data(),
        &shader_include,
        std::string(entry).c_str(),
        std::string(target).c_str(),
        debug ? (D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS) : (D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_WARNINGS_ARE_ERRORS),
        0, // effect flags
        &shader_blob,
        &compile_errors_blob);

    if (FAILED(hr))
    {
        if (compile_errors_blob)
        {
            std::string_view errors = std::string_view(
                reinterpret_cast<const char*>(compile_errors_blob->GetBufferPointer()),
                compile_errors_blob->GetBufferSize());

            for (std::string_view error : ff::string::split(errors, "\r\n"))
            {
                compile_errors.emplace_back(error);
            }
        }

        return nullptr;
    }

    return std::make_shared<ff::internal::data_blob_dx>(shader_blob.Get());
}

static std::shared_ptr<ff::data_base> compile_shader(
    const std::filesystem::path& file_path,
    std::string_view entry,
    std::string_view target,
    const std::unordered_map<std::string_view, std::string_view>& defines,
    bool debug,
    std::vector<std::string>& compile_errors)
{
    std::shared_ptr<ff::data_base> file_data = std::make_shared<ff::data_mem_mapped>(file_path);
    if (file_data->size())
    {
        std::filesystem::path base_path = file_path.parent_path();
        return ::compile_shader(file_data, file_path, base_path, entry, target, defines, debug, compile_errors);
    }

    return nullptr;
}

ff::shader::shader(std::shared_ptr<ff::saved_data_base> saved_data)
    : resource_file(saved_data, ".shader", true)
{}

std::shared_ptr<ff::resource_object_base> ff::internal::shader_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    bool debug = dict.get<bool>("debug");
    std::filesystem::path file_path = dict.get<std::string>("file");
    std::string entry = dict.get<std::string>("entry", "main");
    std::string target = dict.get<std::string>("target");

    std::unordered_map<std::string_view, std::string_view> defines;
    for (auto& i : dict.get<ff::dict>("defines"))
    {
        defines.try_emplace(i.first, i.second->get<std::string>());
    }

    std::vector<std::string> compile_errors;
    std::shared_ptr<ff::data_base> shader_data = ::compile_shader(file_path, entry, target, defines, debug, compile_errors);

    if (!compile_errors.empty())
    {
        for (const std::string& error : compile_errors)
        {
            std::ostringstream str;
            str << "Shader compiler error: " << error;
            context.add_error(str.str());
        }
    }

    auto shader_saved_data = std::make_shared<ff::saved_data_static>(shader_data, shader_data->size(), ff::saved_data_type::none);
    return std::make_shared<ff::shader>(shader_saved_data);
}

std::shared_ptr<ff::resource_object_base> ff::internal::shader_factory::load_from_cache(const ff::dict& dict) const
{
    const ff::resource_object_factory_base* resource_file_factory = ff::resource_object_base::get_factory(typeid(ff::resource_file));
    if (resource_file_factory)
    {
        std::shared_ptr<ff::resource_file> file = std::dynamic_pointer_cast<ff::resource_file>(resource_file_factory->load_from_cache(dict));
        if (file)
        {
            return std::make_shared<ff::shader>(file->saved_data());
        }
    }

    return nullptr;
}
