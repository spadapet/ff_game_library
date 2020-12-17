#pragma once

namespace ff::internal
{
    extern std::string_view RES_BASE;
    extern std::string_view RES_COMPRESS;
    extern std::string_view RES_DEBUG;
    extern std::string_view RES_FILES;
    extern std::string_view RES_IMPORT;
    extern std::string_view RES_LOAD_LISTENER;
    extern std::string_view RES_TEMPLATE;
    extern std::string_view RES_TYPE;
    extern std::string_view RES_VALUES;

    extern std::string_view FILE_PREFIX;
    extern std::string_view LOC_PREFIX;
    extern std::string_view REF_PREFIX;
    extern std::string_view RES_PREFIX;
}

namespace ff
{
    struct load_resources_result
    {
        bool status;
        std::vector<std::string> errors;
        ff::dict dict;
    };

    load_resources_result load_resources_from_file(const std::filesystem::path& path, bool use_cache, bool debug);
    load_resources_result load_resources_from_json(std::string_view json_text, const std::filesystem::path& base_path, bool debug);
    load_resources_result load_resources_from_json(const ff::dict& json_dict, const std::filesystem::path& base_path, bool debug);
    bool save_resources_to_file(const ff::dict& dict, const std::filesystem::path& path);
    bool is_resource_cache_updated(const std::filesystem::path& input_path, const std::filesystem::path& cache_path, bool debug);
}
