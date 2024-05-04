#include "pch.h"

static std::vector<std::string> get_command_line()
{
    std::wstring_view command_line(::GetCommandLine());
    return ff::string::split_command_line(ff::string::to_string(command_line));
}

static void show_usage()
{
    std::cerr << "Command line options:" << std::endl;
    std::cerr << "  1) ff.resource.build.exe -in \"input file\" [-out \"output file\"] [-pdb \"output path\"] [-header \"output C++\"] [-ref \"types.dll\"] [-debug] [-force]" << std::endl;
    std::cerr << "  2) ff.resource.build.exe -combine \"pack file\" -out \"output file\"" << std::endl;
    std::cerr << "  3) ff.resource.build.exe -dump \"pack file\"" << std::endl;
    std::cerr << "  4) ff.resource.build.exe -dumpbin \"pack file\"" << std::endl;
    std::cerr << std::endl;
    std::cerr << "NOTES:" << std::endl;
    std::cerr << "  With -ref, the reference DLL must contain an exported C method: 'void ff_init()'." << std::endl;
    std::cerr << "  Using -dumpbin will save all binary resources to a temp folder and open it." << std::endl;
}

static bool test_load_resources(const ff::dict& dict)
{
    if constexpr (ff::constants::debug_build)
    {
        dict.debug_print();

        ff::resource_objects resources(dict);
        std::forward_list<ff::auto_resource_value> values;

        for (std::string_view name : resources.resource_object_names())
        {
            values.emplace_front(resources.get_resource_object(name));
        }

        resources.flush_all_resources();

        for (auto& value : values)
        {
            if (value->value()->is_type<nullptr_t>())
            {
                std::cerr << "Failed to create resource object: " << value.resource()->name() << std::endl;
                debug_fail_ret_val(false);
            }
        }
    }

    return true;
}

static bool write_header(const std::vector<uint8_t>& data, std::ostream& output, std::string_view cpp_namespace)
{
    const size_t bytes_per_line = 64;
    const char hex_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    output << "namespace " << cpp_namespace << R"(
{
    namespace internal
    {
        static constexpr uint8_t bytes[] =
        {
        )";

    for (const uint8_t* cur = data.data(), *end = data.data() + data.size(); cur < end; cur += bytes_per_line)
    {
        output << "    ";

        for (size_t i = 0, count = std::min<size_t>(bytes_per_line, end - cur); i < count; i++)
        {
            output << "0x" << hex_chars[cur[i] / 16] << hex_chars[cur[i] % 16] << ",";
        }

        output << std::endl << "        ";
    }

    output << R"(};

        static constexpr size_t byte_size = sizeof(bytes);
    }

    static std::shared_ptr<::ff::data_base> data()
    {
        return std::make_shared<::ff::data_static>(internal::bytes, internal::byte_size);
    }
}
)";

    return true;
}

static bool write_symbol_header(const ff::load_resources_result::id_to_name_t& id_to_name, std::ostream& output, std::string_view cpp_namespace)
{
    output << "namespace " << cpp_namespace << std::endl << "{" << std::endl;

    for (const auto& [id, name] : id_to_name)
    {
        output << "    inline constexpr std::string_view " << id << " = \"" << name << "\";" << std::endl;
    }

    output << "}" << std::endl;

    return true;
}

static bool compile_resource_pack(
    const std::filesystem::path& input_file,
    const std::filesystem::path& output_file,
    const std::filesystem::path& pdb_output,
    const std::filesystem::path& header_file,
    const std::filesystem::path& symbol_header_file,
    bool debug)
{
    ff::load_resources_result result = ff::load_resources_from_file(input_file, false, debug);
    if (!result.status || !::test_load_resources(result.dict))
    {
        std::cerr << "Failed to load resources: " << ff::filesystem::to_string(input_file) << std::endl;

        for (auto& error : result.errors)
        {
            std::cerr << error << std::endl;
        }

        return false;
    }

    auto data = std::make_shared<std::vector<uint8_t>>();
    {
        ff::resource_objects resource_objects(result.dict);
        ff::data_writer data_writer(data);
        if (!resource_objects.save(data_writer))
        {
            std::cerr << "Failed to save resources: " << ff::filesystem::to_string(output_file) << std::endl;
            return false;
        }
    }

    if (!output_file.empty())
    {
        ff::file_writer writer(output_file);
        if (!writer || writer.write(data->data(), data->size()) != data->size())
        {
            std::cerr << "Failed to write file: " << ff::filesystem::to_string(output_file) << std::endl;
            return false;
        }
    }

    if (!header_file.empty())
    {
        std::ofstream header_stream(header_file);
        if (!header_stream || !::write_header(*data, header_stream, result.namespace_))
        {
            std::cerr << "Failed to write header file: " << ff::filesystem::to_string(header_file) << std::endl;
            return false;
        }
    }

    if (!symbol_header_file.empty())
    {
        std::ofstream symbol_header_stream(symbol_header_file);
        if (!symbol_header_stream || !::write_symbol_header(result.id_to_name, symbol_header_stream, result.namespace_))
        {
            std::cerr << "Failed to write symbol file: " << ff::filesystem::to_string(header_file) << std::endl;
            return false;
        }
    }

    if (!pdb_output.empty())
    {
        for (const auto& [name, data] : result.output_files)
        {
            std::filesystem::path path = pdb_output / name;
            if (!ff::filesystem::write_binary_file(path, data->data(), data->size()))
            {
                std::cerr << "Failed to write output file: " << ff::filesystem::to_string(path) << std::endl;
                return false;
            }
        }
    }

    return true;
}

static bool combine_resource_packs(const std::vector<std::filesystem::path>& combine_files, const std::filesystem::path& output_file)
{
    ff::resource_objects resource_objects;
    ff::load_resources_result result{};

    for (const std::filesystem::path& file : combine_files)
    {
        ff::file_reader reader(file);
        if (!reader)
        {
            std::cerr << "Failed to open file: " << ff::filesystem::to_string(file) << std::endl;
            return false;
        }

        if (!resource_objects.add_resources(reader))
        {
            std::cerr << "Invalid binary pack file: " << ff::filesystem::to_string(file) << std::endl;
            return false;
        }
    }

    if (!input_file.empty())
    {
        result = ff::load_resources_from_file(input_file, false, debug);
        if (!result.status || !::test_load_resources(result.dict))
        {
            for (auto& error : result.errors)
            {
                std::cerr << error << std::endl;
            }

            return false;
        }
    }

    auto data = std::make_shared<std::vector<uint8_t>>();
    {
        ff::data_writer data_writer(data);
        if (!resource_objects.save(data_writer))
        {
            std::cerr << "Failed to save resource data." << std::endl;
            return false;
        }
    }

    if (!output_file.empty())
    {
        ff::file_writer writer(output_file);
        assert_ret_val(writer && writer.write(data->data(), data->size()) == data->size(), false);
    }

    if (result.status && !header_file.empty())
    {
        std::ofstream header_stream(header_file);
        assert_ret_val(header_stream && ::write_header(*data, header_stream, result.namespace_), false);
    }

    if (result.status && !symbol_header_file.empty())
    {
        std::ofstream symbol_header_stream(symbol_header_file);
        assert_ret_val(symbol_header_stream && ::write_symbol_header(result.id_to_name, symbol_header_stream, result.namespace_), false);
    }

    if (result.status && !pdb_output.empty())
    {
        for (const auto& [name, data] : result.output_files)
        {
            std::filesystem::path path = pdb_output / name;
            assert_ret_val(ff::filesystem::write_binary_file(path, data->data(), data->size()), false);
        }
    }

    return true;
}

class save_to_file_visitor : public ff::dict_visitor_base
{
public:
    save_to_file_visitor()
        : root_path(ff::filesystem::temp_directory_path() / "dumpbin")
    {
        ff::filesystem::create_directories(this->root_path);
    }

    ~save_to_file_visitor()
    {
        if (ff::filesystem::exists(this->root_path))
        {
            const wchar_t* path_str = this->root_path.native().c_str();
            ::ShellExecute(nullptr, L"open", path_str, nullptr, path_str, SW_SHOWDEFAULT);
        }
    }

protected:
    virtual ff::value_ptr transform_dict(const ff::dict& dict) override
    {
        ff::value_ptr new_value = ff::dict_visitor_base::transform_dict(dict);
        ff::value_ptr new_dict_value = ff::type::try_get_dict_from_data(new_value);

        if (new_dict_value)
        {
            const ff::dict& new_dict = new_dict_value->get<ff::dict>();
            std::string type_name = new_dict.get<std::string>(ff::internal::RES_TYPE);
            const ff::resource_object_factory_base* factory = ff::resource_object_base::get_factory(type_name);
            std::shared_ptr<ff::resource_object_base> resource_object = factory ? factory->load_from_cache(new_dict) : nullptr;

            if (resource_object)
            {
                std::filesystem::path name = ff::filesystem::clean_file_name(ff::filesystem::to_path(this->path()));
                resource_object->resource_save_to_file(this->root_path, ff::filesystem::to_string(name));
                return ff::value::create<ff::resource_object_base>(resource_object);
            }
        }

        return new_value;
    }

private:
    std::filesystem::path root_path;
};

static int dump_file(const std::filesystem::path& dump_source_file, bool dump_bin)
{
    ff::init_input init_input;
    ff::init_audio init_audio;
    ff::init_graphics init_graphics;

    if (!ff::filesystem::exists(dump_source_file))
    {
        std::cerr << "File doesn't exist: " << dump_source_file << std::endl;
        return 1;
    }

    ff::file_reader reader(dump_source_file);
    if (!reader)
    {
        std::cerr << "Can't read file: " << dump_source_file << std::endl;
        return 2;
    }

    ff::dict dict;
    ff::resource_objects resources(reader);
    if (!resources.save(dict) || !dict.load_child_dicts())
    {
        std::cerr << "Can't load file: " << dump_source_file << std::endl;
        return 3;
    }

    // console print
    {
        std::ostringstream dict_str;
        dict.print(dict_str);
        std::cout << dict_str.str();
    }

    if (dump_bin)
    {
        ::save_to_file_visitor visitor;
        std::vector<std::string> errors;
        visitor.visit_dict(dict, errors);

        if (!errors.empty())
        {
            for (auto& error : errors)
            {
                std::cerr << error << std::endl;
            }

            return 4;
        }
    }

    return 0;
}

int main()
{
    ff::timer timer;
    ff::init_resource init_resource;

    if (!init_resource)
    {
        std::cerr << "ff.resource.build: Failed to initialize" << std::endl;
        return 5;
    }

    std::vector<std::filesystem::path> refs;
    std::vector<std::filesystem::path> combine_files;
    std::filesystem::path input_file;
    std::filesystem::path output_file;
    std::filesystem::path pdb_output;
    std::filesystem::path header_file;
    std::filesystem::path symbol_header_file;
    std::filesystem::path dump_source_file;
    bool debug = false;
    bool force = false;
    bool verbose = false;
    bool dump_bin = false;

    std::vector<std::string> args = ff::string::split_command_line();
    for (size_t i = 1; i < args.size(); i++)
    {
        std::string_view arg = args[i];

        if (arg == "-in" && i + 1 < args.size())
        {
            input_file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
        }
        else if (arg == "-combine" && i + 1 < args.size())
        {
            combine_files.push_back(std::filesystem::current_path() / ff::filesystem::to_path(args[++i]));
        }
        else if (arg == "-out" && i + 1 < args.size())
        {
            output_file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
        }
        else if (arg == "-pdb" && i + 1 < args.size())
        {
            pdb_output = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
        }
        else if (arg == "-header" && i + 1 < args.size())
        {
            header_file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
        }
        else if (arg == "-symbol_header" && i + 1 < args.size())
        {
            symbol_header_file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
        }
        else if (arg == "-ref" && i + 1 < args.size())
        {
            std::filesystem::path file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
            refs.push_back(file);
        }
        else if ((arg == "-dump" || arg == "-dumpbin") && i + 1 < args.size())
        {
            dump_bin = (arg == "-dumpbin");
            dump_source_file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
        }
        else if (arg == "-debug")
        {
            debug = true;
        }
        else if (arg == "-force")
        {
            force = true;
        }
        else if (arg == "-verbose")
        {
            verbose = true;
        }
        else
        {
            ::show_usage();
            return 1;
        }
    }

    if (!dump_source_file.empty())
    {
        if (!input_file.empty() || !output_file.empty() || !combine_files.empty())
        {
            ::show_usage();
            return 1;
        }

        return ::dump_file(dump_source_file, dump_bin);
    }

    if (input_file.empty() == combine_files.empty())
    {
        ::show_usage();
        return 1;
    }

    if (output_file.empty())
    {
        if (input_file.empty())
        {
            ::show_usage();
            return 1;
        }

        output_file = input_file;
        output_file.replace_extension(".pack");
    }

    if (!input_file.empty() && !ff::filesystem::exists(input_file))
    {
        std::cerr << "ff.resource.build: File doesn't exist: " << input_file << std::endl;
        return 2;
    }

    for (auto& file : combine_files)
    {
        if (!ff::filesystem::exists(file))
        {
            std::cerr << "ff.resource.build: File doesn't exist: " << file << std::endl;
            return 2;
        }

        std::cout << "Combine: " << ff::filesystem::to_string(file) << std::endl;
    }

    if (!combine_files.empty())
    {
        force = true;
    }

    bool skipped = !force && ff::is_resource_cache_updated(input_file, output_file);
    std::cout << ff::filesystem::to_string(input_file) << " -> " << ff::filesystem::to_string(output_file) << (skipped ? " (skipped)" : "") << std::endl;

    if (!skipped)
    {
        ff::init_input init_input;
        ff::init_audio init_audio;
        ff::init_graphics init_graphics;

        if (!init_graphics || !init_audio || !init_input)
        {
            std::cerr << "ff.resource.build: Failed to initialize" << std::endl;
            return 5;
        }

        // Load referenced DLLs
        for (auto& ref : refs)
        {
            HMODULE mod = ::LoadLibrary(ref.c_str());
            if (mod)
            {
                typedef void (*ff_init_t)();
                ff_init_t init_func = reinterpret_cast<ff_init_t>(::GetProcAddress(mod, "ff_init"));

                if (verbose)
                {
                    std::cout << "ff.resource.build: Loaded: " << ref << std::endl;
                }

                if (!init_func)
                {
                    std::cerr << "ff.resource.build: Reference doesn't contain 'ff_init' export: " << ref << std::endl;
                    return 3;
                }

                init_func();
            }
            else
            {
                std::cerr << "ff.resource.build: Failed to load reference: " << ref << std::endl;
                return 4;
            }
        }

        if (!::compile_resource_pack(combine_files, input_file, output_file, pdb_output, header_file, symbol_header_file, debug))
        {
            std::cerr << "ff.resource.build: Compile failed" << std::endl;
            return 6;
        }
    }

    if (verbose)
    {
        std::cout <<
            "ff.resource.build: Time: " <<
            std::fixed <<
            std::setprecision(3) <<
            timer.tick() <<
            "s (" << input_file << ")" <<
            std::endl;
    }

    return 0;
}
