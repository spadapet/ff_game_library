#include "pch.h"
#include "resource_load.h"
#include "resource_objects.h"

static std::filesystem::path get_cache_path(const std::filesystem::path& source_path, bool debug)
{
    std::filesystem::path path_canon = ff::filesystem::weakly_canonical(source_path);
    std::filesystem::path path_canon_lower = ff::filesystem::to_lower(path_canon);
    std::filesystem::path name = path_canon.filename().replace_extension();

    std::ostringstream str;
    str << ff::filesystem::to_string(name) << "." << ff::stable_hash_func(path_canon_lower) << (debug ? ".debug" : "") << ".pack";
    std::filesystem::path cache_path = ff::filesystem::user_local_path();
    return (cache_path /= "ff.cache") /= str.str();
}

// mem-mapping the file will lock it on disk, not allowing the cache to be updated
static std::shared_ptr<ff::resource_objects> load_cached_resources(const std::filesystem::path& path, bool mem_map_file)
{
    if (!ff::filesystem::exists(path))
    {
        return {};
    }

    std::filesystem::file_time_type time = ff::filesystem::last_write_time(path);
    if (time == std::filesystem::file_time_type::min())
    {
        return {};
    }

    auto data = mem_map_file ? ff::filesystem::map_binary_file(path) : ff::filesystem::read_binary_file(path);
    ff::data_reader reader(data);
    auto resource_objects = std::make_shared<ff::resource_objects>();
    if (!data || !resource_objects->add_resources(reader))
    {
        return {};
    }

    std::vector<std::string> file_refs = resource_objects->input_files();
    assert_ret_val(file_refs.size(), std::shared_ptr<ff::resource_objects>());

    for (const std::string& file_ref : file_refs)
    {
        std::filesystem::path path_ref = ff::filesystem::to_path(file_ref);
        if (!ff::filesystem::exists(path_ref))
        {
            // cache is out of data
            return {};
        }

        std::filesystem::file_time_type file_ref_time = ff::filesystem::last_write_time(path_ref);
        if (file_ref_time == std::filesystem::file_time_type::min() || time < file_ref_time)
        {
            // cache is out of data
            return {};
        }
    }

    return resource_objects;
}

ff::load_resources_result ff::load_resources_from_file(const std::filesystem::path& path, bool use_cache, bool debug)
{
    ff::load_resources_result result{};
    std::filesystem::path cache_path = use_cache ? ::get_cache_path(path, debug) : std::filesystem::path();

    if (use_cache)
    {
        auto resource_objects = ::load_cached_resources(cache_path, false);
        if (resource_objects)
        {
            result.resources = resource_objects;
        }

        return result;
    }

    std::string text;
    if (ff::filesystem::read_text_file(path, text))
    {
        std::filesystem::path base_path = path.parent_path();
        result = ff::load_resources_from_json(text, base_path, debug);
        if (result.resources && debug)
        {
            // Remember the source
            std::vector<std::string> files;
            files.push_back(ff::filesystem::to_string(path));

            ff::dict resource_metadata;
            resource_metadata.set<std::vector<std::string>>(ff::internal::RES_SOURCES, std::vector<std::string>(files));
            resource_metadata.set<std::vector<std::string>>(ff::internal::RES_FILES, std::vector<std::string>(files));
            result.resources->add_resources(resource_metadata);

            if (use_cache)
            {
                // Ignore failures saving the cache, just delete the cache instead
                ff::file_writer writer(cache_path);
                if (writer && !result.resources->save(writer))
                {
                    ff::filesystem::remove(cache_path);
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
        str << "Failed parsing JSON at pos: " << i << "\r\n  -->" << json_text.substr(i, std::min<size_t>(32, json_text.size() - i));

        ff::load_resources_result result{};
        result.errors.push_back(str.str());
        return result;
    }

    return ff::load_resources_from_json(dict, base_path, debug);
}

bool ff::is_resource_cache_updated(const std::filesystem::path& input_path, const std::filesystem::path& cache_path)
{
    auto cached_resources = ::load_cached_resources(cache_path, true);
    if (cached_resources)
    {
        std::filesystem::file_time_type input_time = std::filesystem::last_write_time(input_path);
        std::filesystem::file_time_type cache_time = std::filesystem::last_write_time(cache_path);

        // We know all referenced files are still valid, but make sure the input file is actually referenced
        if (input_time != std::filesystem::file_time_type::min() &&
            cache_time != std::filesystem::file_time_type::min() &&
            cache_time >= input_time)
        {
            std::vector<std::string> files = cached_resources->input_files();
            for (const std::string& file : files)
            {
                if (ff::filesystem::equivalent(input_path, std::filesystem::path(file)))
                {
                    return true;
                }
            }
        }
    }

    return false;
}
