#include "pch.h"
#include "base/stable_hash.h"
#include "data_value/string_v.h"
#include "data_persist/file.h"
#include "data_persist/filesystem.h"
#include "data_persist/json_persist.h"
#include "data_persist/stream.h"
#include "resource/resource_load.h"
#include "resource/resource_objects.h"

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
    std::filesystem::file_time_type time = ff::filesystem::last_write_time(path);
    if (time == std::filesystem::file_time_type::min())
    {
        return {};
    }

    auto data = mem_map_file ? ff::filesystem::map_binary_file(path) : ff::filesystem::read_binary_file(path);
    assert_ret_val(data, std::shared_ptr<ff::resource_objects>());

    ff::data_reader reader(data);
    auto resource_objects = std::make_shared<ff::resource_objects>();
    assert_ret_val(resource_objects->add_resources(reader), std::shared_ptr<ff::resource_objects>());

    std::vector<std::string> input_files = resource_objects->input_files();
    check_ret_val(input_files.size(), std::shared_ptr<ff::resource_objects>());

    for (const std::string& file_ref : input_files)
    {
        std::filesystem::path path_ref = ff::filesystem::to_path(file_ref);
        std::filesystem::file_time_type file_ref_time = ff::filesystem::last_write_time(path_ref);
        if (file_ref_time == std::filesystem::file_time_type::min() || time < file_ref_time)
        {
            // cache is out of data
            return {};
        }
    }

    return resource_objects;
}

ff::load_resources_result ff::load_resources_from_file(const std::filesystem::path& path, ff::resource_cache_t cache_type, bool debug)
{
    ff::load_resources_result result{};

    if (cache_type != ff::resource_cache_t::none && cache_type != ff::resource_cache_t::rebuild_cache)
    {
        result.cache_path = ::get_cache_path(path, debug);
        result.resources = ::load_cached_resources(result.cache_path, cache_type == ff::resource_cache_t::use_cache_mem_mapped);
        if (result.resources)
        {
            result.loaded_from_cache = true;
            return result;
        }
    }

    std::string text;
    if (ff::filesystem::read_text_file(path, text))
    {
        std::filesystem::path base_path = path.parent_path();
        result = ff::load_resources_from_json(text, base_path, debug);
        if (result.resources)
        {
            if (debug)
            {
                // Remember the source
                std::vector<std::string> files;
                files.push_back(ff::filesystem::to_string(path));

                ff::dict resource_metadata;
                resource_metadata.set<std::vector<std::string>>(ff::internal::RES_SOURCES, std::vector<std::string>(files));
                resource_metadata.set<std::vector<std::string>>(ff::internal::RES_FILES, std::vector<std::string>(files));
                result.resources->add_resources(resource_metadata);
            }

            if (cache_type != ff::resource_cache_t::none)
            {
                result.cache_path = ::get_cache_path(path, debug);

                ff::file_writer writer(result.cache_path);
                if (writer && !result.resources->save(writer))
                {
                    // Ignore failures saving the cache, just delete the cache instead
                    ff::filesystem::remove(result.cache_path);
                    result.cache_path.clear();
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

bool ff::is_resource_cache_updated(const std::vector<std::filesystem::path>& source_files, const std::filesystem::path& cache_path)
{
    auto cached_resources = ::load_cached_resources(cache_path, true);
    check_ret_val(cached_resources, false);

    // We know all referenced files are still valid, but make sure the source files match
    std::vector<std::string> check_strings = cached_resources->source_files();
    if (check_strings.size() != source_files.size())
    {
        return false;
    }

    for (auto& check_string : cached_resources->source_files())
    {
        std::filesystem::path check_file = ff::filesystem::to_path(check_string);

        auto i = std::find_if(source_files.cbegin(), source_files.cend(),
            [&check_file](const std::filesystem::path& source_file)
            {
                return std::filesystem::equivalent(source_file, check_file);
            });

        if (i == source_files.cend())
        {
            return false;
        }
    }

    return true;
}
