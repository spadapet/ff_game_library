#include "pch.h"
#include "resource_load.h"

std::string_view ff::internal::RES_BASE("res:base");
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

static std::filesystem::path get_cache_path(const std::filesystem::path& source_path, bool debug)
{
    std::filesystem::path path_canon = std::filesystem::canonical(source_path);
    std::filesystem::path path_canon_lower = ff::filesystem::to_lower(path_canon);
    std::filesystem::path name = path_canon.filename().replace_extension();

    std::ostringstream str;
    str << name << "." << ff::hash<std::filesystem::path>()(path_canon_lower) << (debug ? ".debug" : "") << ".pack";
    std::filesystem::path cache_path = ff::filesystem::user_directory_path();
    return (cache_path /= "cached_resources") /= str.str();
}

static bool load_cached_resources(const std::filesystem::path& path, ff::dict& dict)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
    {
        return false;
    }

    std::filesystem::file_time_type time = std::filesystem::last_write_time(path, ec);
    if (time == std::filesystem::file_time_type::min())
    {
        return false;
    }

    auto data = std::make_shared<ff::data_mem_mapped>(ff::file_mem_mapped(path));
    if (!data->valid() || !ff::dict::load(ff::data_reader(data), dict))
    {
        return false;
    }

    std::vector<std::string> file_refs = dict.get<std::vector<std::string>>(ff::internal::RES_FILES);
    for (const std::string& file_ref : file_refs)
    {
        std::filesystem::path path_ref(file_ref);
        if (!std::filesystem::exists(path_ref, ec))
        {
            // cache is out of data
            return false;
        }

        std::filesystem::file_time_type file_ref_time = std::filesystem::last_write_time(path_ref, ec);
        if (file_ref_time == std::filesystem::file_time_type::min() || file_ref_time < time)
        {
            // cache is out of data
            return false;
        }
    }

    return true;
}

ff::load_resources_result ff::load_resources_from_file(const std::filesystem::path& path, bool use_cache, bool debug)
{
    ff::load_resources_result result{};
    std::filesystem::path cache_path = use_cache ? ::get_cache_path(path, debug) : std::filesystem::path();

    if (use_cache && ::load_cached_resources(cache_path, result.dict))
    {
        result.status = true;
        return result;
    }

    std::string text;
    if (ff::filesystem::read_text_file(path, text))
    {
        std::filesystem::path base_path = path.parent_path();
        result = ff::load_resources_from_json(text, base_path, debug);
        if (result.status)
        {
            // Add the text file to the list of input files
            std::vector<std::string> files = result.dict.get<std::vector<std::string>>(ff::internal::RES_FILES);
            files.push_back(path.string());
            result.dict.set<std::vector<std::string>>(ff::internal::RES_FILES, std::move(files));

            if (use_cache)
            {
                // Ignore failures saving the cache, just delete the cache instead
                ff::file_writer writer(cache_path);
                if (writer && !result.dict.save(writer))
                {
                    std::error_code ec;
                    std::filesystem::remove(cache_path, ec);
                }
            }
        }
    }

    return result;
}

ff::load_resources_result ff::load_resources_from_json(std::string_view json_text, const std::filesystem::path& base_path, bool debug)
{
    const char* error_pos;
    ff::dict dict;
    if (!ff::json_parse(json_text, dict, &error_pos))
    {
        std::ostringstream str;
        size_t i = error_pos - json_text.data();
        str << "Failed parsing JSON at pos: " << i << std::endl << "  -->" << json_text.substr(i, std::min<size_t>(32, json_text.size() - i));

        ff::load_resources_result result{};
        result.errors.push_back(str.str());
        return result;
    }

    return ff::load_resources_from_json(dict, base_path, debug);
}

bool ff::is_resource_cache_updated(const std::filesystem::path& input_path, const std::filesystem::path& cache_path)
{
    ff::dict dict;
    if (::load_cached_resources(cache_path, dict))
    {
        std::error_code ec;
        std::filesystem::file_time_type input_time = std::filesystem::last_write_time(input_path);
        std::filesystem::file_time_type cache_time = std::filesystem::last_write_time(cache_path);

        // We know all referenced files are still valid, but make sure the input file is actually referenced
        if (input_time != std::filesystem::file_time_type::min() &&
            cache_time != std::filesystem::file_time_type::min() &&
            cache_time >= input_time)
        {
            std::vector<std::string> files = dict.get<std::vector<std::string>>(ff::internal::RES_FILES);
            for (const std::string& file : files)
            {
                if (std::filesystem::equivalent(input_path, std::filesystem::path(file), ec))
                {
                    return true;
                }
            }
        }
    }

    return false;
}
