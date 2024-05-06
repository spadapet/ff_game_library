#pragma once

namespace ff::internal
{
    extern std::string_view RES_FILES;
    extern std::string_view RES_ID_SYMBOLS;
    extern std::string_view RES_IMPORT;
    extern std::string_view RES_OUTPUT_FILES;
    extern std::string_view RES_NAMESPACE;
    extern std::string_view RES_SYMBOL;
    extern std::string_view RES_SOURCE;
    extern std::string_view RES_SOURCES;
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
    class resource_objects;

    struct load_resources_result
    {
        std::shared_ptr<ff::resource_objects> resources;
        std::vector<std::string> errors;
    };

    ff::load_resources_result load_resources_from_file(const std::filesystem::path& path, bool use_cache, bool debug);
    ff::load_resources_result load_resources_from_json(std::string_view json_text, const std::filesystem::path& base_path, bool debug);
    ff::load_resources_result load_resources_from_json(const ff::dict& json_dict, const std::filesystem::path& base_path, bool debug);
    bool is_resource_cache_updated(const std::filesystem::path& input_path, const std::filesystem::path& cache_path);
}
