#pragma once

namespace ff::internal
{
    extern std::string_view RES_FILES;
    extern std::string_view RES_IMPORT;
    extern std::string_view RES_NAMESPACE;
    extern std::string_view RES_SYMBOL;
    extern std::string_view RES_SOURCE;
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
        using output_files_t = typename std::vector<std::pair<std::string, std::shared_ptr<ff::data_base>>>;
        using id_to_name_t = typename std::vector<std::pair<std::string, std::string>>;

        bool status;
        std::vector<std::string> errors;
        std::string namespace_;
        ff::dict dict;
        output_files_t output_files;
        id_to_name_t id_to_name;
    };

    load_resources_result load_resources_from_file(const std::filesystem::path& path, bool use_cache, bool debug);
    load_resources_result load_resources_from_json(std::string_view json_text, const std::filesystem::path& base_path, bool debug);
    load_resources_result load_resources_from_json(const ff::dict& json_dict, const std::filesystem::path& base_path, bool debug);
    bool is_resource_cache_updated(const std::filesystem::path& input_path, const std::filesystem::path& cache_path);
}
