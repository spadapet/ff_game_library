#include "pch.h"
#include "resource_persist.h"

std::string_view ff::internal::RES_BASE("res:base");
std::string_view ff::internal::RES_COMPRESS("res:compress");
std::string_view ff::internal::RES_DEBUG("res:debug");
std::string_view ff::internal::RES_FILES("res:files");
std::string_view ff::internal::RES_IMPORT("res:import");
std::string_view ff::internal::RES_LOAD_LISTENER("res:loadListener");
std::string_view ff::internal::RES_TEMPLATE("res:template");
std::string_view ff::internal::RES_TYPE("res:type");
std::string_view ff::internal::RES_VALUES("res:values");

std::string_view ff::internal::FILE_PREFIX("file:");
std::string_view ff::internal::LOC_PREFIX("loc:");
std::string_view ff::internal::REF_PREFIX("ref:");
std::string_view ff::internal::RES_PREFIX("res:");

ff::load_resources_result ff::load_resources_from_file(const std::filesystem::path& path, bool use_cache, bool debug)
{
    return load_resources_result();
}

ff::load_resources_result ff::load_resources_from_json(std::string_view json_text, const std::filesystem::path& base_path, bool debug)
{
    return load_resources_result();
}

ff::load_resources_result ff::load_resources_from_json(const ff::dict& json_dict, const std::filesystem::path& base_path, bool debug)
{
    return load_resources_result();
}

bool ff::save_resources_to_file(const ff::dict& dict, const std::filesystem::path& path)
{
    return false;
}

bool ff::is_resource_cache_updated(const std::filesystem::path& input_path, const std::filesystem::path& cache_path, bool debug)
{
    return false;
}
