#pragma once

namespace ff::internal
{
    inline constexpr std::string_view RES_FILES = "res:files";
    inline constexpr std::string_view RES_ID_SYMBOLS = "res:id_symbols";
    inline constexpr std::string_view RES_IMPORT = "res:import";
    inline constexpr std::string_view RES_OUTPUT_FILES = "res:output_files";
    inline constexpr std::string_view RES_METADATA = "res:metadata";
    inline constexpr std::string_view RES_NAMESPACE = "res:namespace";
    inline constexpr std::string_view RES_NAMESPACES = "res:namespaces";
    inline constexpr std::string_view RES_SYMBOL = "res:symbol";
    inline constexpr std::string_view RES_SOURCE = "res:source";
    inline constexpr std::string_view RES_SOURCES = "res:sources";
    inline constexpr std::string_view RES_TEMPLATE = "res:template";
    inline constexpr std::string_view RES_TYPE = "res:type";
    inline constexpr std::string_view RES_VALUES = "res:values";

    inline constexpr std::string_view FILE_PREFIX = "file:";
    inline constexpr std::string_view LOC_PREFIX = "loc:";
    inline constexpr std::string_view REF_PREFIX = "ref:";
    inline constexpr std::string_view RES_PREFIX = "res:";
}

namespace ff
{
    class dict;
    class resource_objects;

    struct load_resources_result
    {
        std::shared_ptr<ff::resource_objects> resources;
        std::vector<std::string> errors;
        std::filesystem::path cache_path;
        bool loaded_from_cache{};
    };

    enum class resource_cache_t
    {
        none,
        use_cache_in_memory,
        use_cache_mem_mapped,
        rebuild_cache,
    };

    ff::load_resources_result load_resources_from_file(const std::filesystem::path& path, ff::resource_cache_t cache_type, bool debug);
    ff::load_resources_result load_resources_from_json(std::string_view json_text, const std::filesystem::path& base_path, bool debug);
    ff::load_resources_result load_resources_from_json(const ff::dict& json_dict, const std::filesystem::path& base_path, bool debug);
    bool is_resource_cache_updated(const std::vector<std::filesystem::path>& source_files, const std::filesystem::path& cache_path);
}
