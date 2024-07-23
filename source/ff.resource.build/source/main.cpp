#include "pch.h"

constexpr int EXIT_CODE_SUCCESS = 0;
constexpr int EXIT_CODE_BAD_COMMAND_LINE = 1;
constexpr int EXIT_CODE_BAD_INPUT = 2;
constexpr int EXIT_CODE_NOT_IMPLEMENTED = 3;
constexpr int EXIT_CODE_BAD_REFERENCE = 4;
constexpr int EXIT_CODE_INIT_FAILED = 5;
constexpr int EXIT_CODE_COMPILE_FAILED = 6;
constexpr int EXIT_CODE_OPEN_FILE_FAILED = 7;
constexpr int EXIT_CODE_READ_FILE_FAILED = 8;
constexpr int EXIT_CODE_SAVE_DICT_FAILED = 9;
constexpr int EXIT_CODE_VISIT_DICT_FAILED = 10;
constexpr std::string_view PROGRAM_NAME = "ff.resource.build";
constexpr std::string_view ASSETS_COMBINED_NAMESPACE = "assets_combined";

static int show_usage()
{
    std::cerr << "Command line options:\n";
    std::cerr << "  1) " << ::PROGRAM_NAME << ".exe -in \"input file\" [-out \"output file\"] [-pdb \"output path\"] [-header \"output C++\"] [-ref \"types.dll\"] [-debug] [-force]\n";
    std::cerr << "  3) " << ::PROGRAM_NAME << ".exe -dump \"pack file\"\n";
    std::cerr << "  4) " << ::PROGRAM_NAME << ".exe -dumpbin \"pack file\"\n\n";
    std::cerr << "NOTES:\n";
    std::cerr << "  -verbose can be added to any command for extra log output.\n";
    std::cerr << "  With -ref, the reference DLL must contain an exported C method: 'void ff_init()'.\n";
    std::cerr << "  Using -dumpbin will save all binary resources to a temp folder and open it.\n";

    return ::EXIT_CODE_BAD_COMMAND_LINE;
}

static bool write_header(const ff::data_base& data, std::ostream& output, std::string_view cpp_namespace)
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

        output << "\n        ";
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

static bool write_symbol_header(const std::vector<std::pair<std::string, std::string>>& id_to_names, std::ostream& output, std::string_view cpp_namespace)
{
    output << "namespace " << cpp_namespace << "\n{\n";

    for (const auto& [id, name] : id_to_names)
    {
        output << "    inline constexpr std::string_view " << id << " = \"" << name << "\";\n";
    }

    output << "}\n\n";

    return true;
}

static void test_load_resources(const ff::resource_objects& resources)
{
    if constexpr (ff::constants::debug_build)
    {
        ff::dict dict;
        assert_ret(resources.save(dict));
        dict.debug_print();

        ff::resource_objects resources_copy(resources);
        std::forward_list<ff::auto_resource_value> values;

        for (std::string_view name : resources_copy.resource_object_names())
        {
            values.emplace_front(resources_copy.get_resource_object(name));
        }

        resources_copy.flush_all_resources();

        for (auto& value : values)
        {
            if (value->value()->is_type<nullptr_t>())
            {
                std::cerr << ::PROGRAM_NAME << ": Failed to create resource object: " << value.resource()->name() << "\n";
                debug_fail_ret();
            }
        }
    }
}

static bool compile_resource_pack(
    const std::vector<std::filesystem::path>& input_files,
    const std::filesystem::path& output_file,
    const std::filesystem::path& pdb_output,
    const std::filesystem::path& header_file,
    const std::filesystem::path& symbol_header_file,
    const bool force,
    const bool debug)
{
    assert_ret_val(!input_files.empty(), false);

    std::vector<ff::load_resources_result> load_results;
    load_results.reserve(input_files.size());

    for (const std::filesystem::path& input_file : input_files)
    {
        const ff::resource_cache_t cache_type = force ? ff::resource_cache_t::rebuild_cache : ff::resource_cache_t::use_cache_mem_mapped;
        ff::load_resources_result result = ff::load_resources_from_file(input_file, cache_type, debug);
        if (result.errors.empty() && result.resources)
        {
            load_results.push_back(std::move(result));
        }
        else
        {
            std::cerr << "Failed to load resources: " << ff::filesystem::to_string(input_file) << "\n";

            for (auto& error : result.errors)
            {
                std::cerr << error << "\n";
            }
        }
    }

    check_ret_val(load_results.size() == input_files.size(), false);

    ff::resource_objects built_resources;
    {
        for (const ff::load_resources_result& result : load_results)
        {
            built_resources.add_resources(*result.resources);
        }

        ::test_load_resources(built_resources);
    }

    auto source_namespaces = built_resources.source_namespaces();
    auto source_output_files = built_resources.output_files();

    if (!output_file.empty())
    {
        bool written = false;
        bool use_single_cache = load_results.size() == 1 && !load_results.front().cache_path.empty();

        if (use_single_cache)
        {
            std::error_code ec{};
            written = std::filesystem::copy_file(load_results.front().cache_path, output_file,
                std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::create_hard_links, ec);
        }

        if (use_single_cache && !written)
        {
            std::error_code ec{};
            written = std::filesystem::copy_file(load_results.front().cache_path, output_file,
                std::filesystem::copy_options::overwrite_existing, ec);
        }

        if (!written)
        {
            ff::file_writer writer(output_file);
            if (!writer)
            {
                std::cerr << "Failed to create file: " << ff::filesystem::to_string(output_file) << "\n";
                return false;
            }

            if (!built_resources.save(writer))
            {
                std::cerr << "Failed to write file: " << ff::filesystem::to_string(output_file) << "\n";
                return false;
            }
        }
    }

    if (!output_file.empty() && !header_file.empty() && source_namespaces.size())
    {
        std::ofstream header_stream(header_file);
        ff::data_mem_mapped output_data(output_file);
        std::string source_namespace = (source_namespaces.size() == 1) ? source_namespaces.front() : std::string(::ASSETS_COMBINED_NAMESPACE);
        if (!header_stream || !output_data.valid() || !::write_header(output_data, header_stream, source_namespace))
        {
            std::cerr << "Failed to write header file: " << ff::filesystem::to_string(header_file) << "\n";
            return false;
        }
    }

    if (!symbol_header_file.empty() && source_namespaces.size())
    {
        std::ofstream symbol_header_stream(symbol_header_file);
        if (!symbol_header_stream)
        {
            std::cerr << "Failed to create symbol file: " << ff::filesystem::to_string(header_file) << "\n";
            return false;
        }

        for (const std::string& source_namespace : source_namespaces)
        {
            auto source_id_to_names = built_resources.id_to_names(source_namespace);
            if (!source_id_to_names.empty() && !::write_symbol_header(source_id_to_names, symbol_header_stream, source_namespace))
            {
                std::cerr << "Failed to write symbol file: " << ff::filesystem::to_string(header_file) << "\n";
                return false;
            }
        }
    }

    if (!pdb_output.empty() && source_output_files.size())
    {
        for (const auto& [name, data] : source_output_files)
        {
            std::filesystem::path path = pdb_output / name;
            if (!ff::filesystem::write_binary_file(path, data->data(), data->size()))
            {
                std::cerr << "Failed to write output file: " << ff::filesystem::to_string(path) << "\n";
                return false;
            }
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

static bool load_reference_files(const std::vector<std::filesystem::path>& reference_files, bool verbose)
{
    for (auto& ref : reference_files)
    {
        HMODULE mod = ::LoadLibrary(ref.c_str());
        if (mod)
        {
            typedef void (*ff_init_t)();
            ff_init_t init_func = reinterpret_cast<ff_init_t>(::GetProcAddress(mod, "ff_init"));

            if (verbose)
            {
                std::cout << ::PROGRAM_NAME << ": Loaded reference: " << ref << "\n";
            }

            if (!init_func)
            {
                std::cerr << ::PROGRAM_NAME << ": Reference doesn't contain 'ff_init' export: " << ref << "\n";
                return false;
            }

            init_func();
        }
        else
        {
            std::cerr << ::PROGRAM_NAME << ": Failed to load reference: " << ref << "\n";
            return false;
        }
    }

    return true;
}

static int do_compile(
    const std::vector<std::filesystem::path>& input_files,
    const std::filesystem::path& output_file,
    const std::vector<std::filesystem::path>& reference_files,
    const std::filesystem::path& pdb_output,
    const std::filesystem::path& header_file,
    const std::filesystem::path& symbol_header_file,
    const bool force,
    const bool debug,
    const bool verbose)
{
    bool skipped = !force && ff::is_resource_cache_updated(input_files, output_file);

    for (auto& input_file : input_files)
    {
        std::cout << ff::filesystem::to_string(input_file) << "\n";
    }

    std::cout << "  -> " << (skipped ? "(skipped) " : "") << ff::filesystem::to_string(output_file) << "\n";
    check_ret_val(!skipped, ::EXIT_CODE_SUCCESS);

    ff::init_input init_input;
    ff::init_audio init_audio;
    ff::init_graphics init_graphics;

    if (!init_graphics || !init_audio || !init_input)
    {
        std::cerr << ::PROGRAM_NAME << ": Failed to initialize\n";
        return ::EXIT_CODE_INIT_FAILED;
    }

    if (!::load_reference_files(reference_files, verbose))
    {
        return ::EXIT_CODE_BAD_REFERENCE;
    }

    if (!::compile_resource_pack(input_files, output_file, pdb_output, header_file, symbol_header_file, force, debug))
    {
        std::cerr << ::PROGRAM_NAME << ": Compile failed\n";
        return ::EXIT_CODE_COMPILE_FAILED;
    }

    return ::EXIT_CODE_SUCCESS;
}

static int do_combine(const std::vector<std::filesystem::path>& input_files, const std::filesystem::path& output_file, bool debug, bool verbose)
{
    std::cerr << ::PROGRAM_NAME << ": Combine is not implemented.";
    return ::EXIT_CODE_NOT_IMPLEMENTED;
}

static int do_dump(const std::filesystem::path& input_file, bool dump_bin)
{
    ff::init_input init_input;
    ff::init_audio init_audio;
    ff::init_graphics init_graphics;

    if (!init_graphics || !init_audio || !init_input)
    {
        std::cerr << ::PROGRAM_NAME << ": Failed to initialize\n";
        return ::EXIT_CODE_INIT_FAILED;
    }

    ff::file_reader reader(input_file);
    if (!reader)
    {
        std::cerr << "Can't open file: " << input_file << "\n";
        return ::EXIT_CODE_OPEN_FILE_FAILED;
    }

    ff::resource_objects resources;
    if (!resources.add_resources(reader))
    {
        std::cerr << "Can't load file: " << input_file << "\n";
        return ::EXIT_CODE_READ_FILE_FAILED;
    }

    ff::dict dict;
    if (!resources.save(dict) || !dict.load_child_dicts())
    {
        std::cerr << "Can't load resources: " << input_file << "\n";
        return ::EXIT_CODE_SAVE_DICT_FAILED;
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
                std::cerr << ::PROGRAM_NAME << ": " << error << "\n";
            }

            return ::EXIT_CODE_VISIT_DICT_FAILED;
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
        std::cerr << ::PROGRAM_NAME << ": Failed to initialize\n";
        return ::EXIT_CODE_INIT_FAILED;
    }

    enum class command_t
    {
        none,
        compile,
        dump_text,
        dump_binary,
    } command = command_t::none;

    enum class command_flags_t
    {
        none = 0x00,
        debug = 0x01,
        force = 0x02,
        verbose = 0x04,
    } command_flags = command_flags_t::none;

    std::vector<std::filesystem::path> reference_files;
    std::vector<std::filesystem::path> input_files;
    std::filesystem::path output_file;
    std::filesystem::path pdb_output;
    std::filesystem::path header_file;
    std::filesystem::path symbol_header_file;

    auto at_exit = ff::scope_exit([&timer, &command_flags]()
    {
        if (ff::flags::has(command_flags, command_flags_t::verbose))
        {
            std::cout << ::PROGRAM_NAME << ": Time: " << std::fixed << std::setprecision(3) << timer.tick() << "s\n";
        }
    });

    // Parse command line
    {
        const std::vector<std::string> args = ff::string::split_command_line();
        for (size_t i = 1; i < args.size(); i++)
        {
            std::string_view arg = args[i];

            if (arg == "-in" && i + 1 < args.size())
            {
                if (command != command_t::none && command != command_t::compile)
                {
                    return ::show_usage();
                }

                command = command_t::compile;
                input_files.push_back(std::filesystem::current_path() / ff::filesystem::to_path(args[++i]));
            }
            else if (arg == "-out" && i + 1 < args.size())
            {
                if (command != command_t::compile)
                {
                    return ::show_usage();
                }

                output_file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
            }
            else if (arg == "-pdb" && i + 1 < args.size())
            {
                if (command != command_t::compile)
                {
                    return ::show_usage();
                }

                pdb_output = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
            }
            else if (arg == "-header" && i + 1 < args.size())
            {
                if (command != command_t::compile)
                {
                    return ::show_usage();
                }

                header_file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
            }
            else if (arg == "-symbol_header" && i + 1 < args.size())
            {
                if (command != command_t::compile)
                {
                    return ::show_usage();
                }

                symbol_header_file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
            }
            else if (arg == "-ref" && i + 1 < args.size())
            {
                if (command != command_t::compile)
                {
                    return ::show_usage();
                }

                reference_files.push_back(std::filesystem::current_path() / ff::filesystem::to_path(args[++i]));
            }
            else if ((arg == "-dump" || arg == "-dumpbin") && i + 1 < args.size())
            {
                if (command != command_t::none || !input_files.empty())
                {
                    return ::show_usage();
                }

                command = (arg == "-dumpbin") ? command_t::dump_binary : command_t::dump_text;
                input_files.push_back(std::filesystem::current_path() / ff::filesystem::to_path(args[++i]));
            }
            else if (arg == "-debug")
            {
                command_flags = ff::flags::combine(command_flags, command_flags_t::debug);
            }
            else if (arg == "-force")
            {
                command_flags = ff::flags::combine(command_flags, command_flags_t::force);
            }
            else if (arg == "-verbose")
            {
                command_flags = ff::flags::combine(command_flags, command_flags_t::verbose);
            }
            else
            {
                return ::show_usage();
            }
        }
    }

    const bool force = ff::flags::has(command_flags, command_flags_t::force);
    const bool debug = ff::flags::has(command_flags, command_flags_t::debug);
    const bool verbose = ff::flags::has(command_flags, command_flags_t::verbose);

    // Validate args
    for (const std::filesystem::path& file : input_files)
    {
        if (!ff::filesystem::exists(file))
        {
            std::cerr << ::PROGRAM_NAME << ": Input file doesn't exist: " << ff::filesystem::to_string(file) << "\n";
            return ::EXIT_CODE_BAD_INPUT;
        }

        if (verbose)
        {
            std::cout << "Input file: " << ff::filesystem::to_string(file) << "\n";
        }

        if (output_file.empty())
        {
            output_file = file;
            output_file.replace_extension(".pack");
        }
    }

    if (verbose && !output_file.empty())
    {
        std::cout << "Output file: " << ff::filesystem::to_string(output_file) << "\n";
    }

    // Run command
    switch (command)
    {
        case command_t::compile:
            return ::do_compile(input_files, output_file, reference_files, pdb_output, header_file, symbol_header_file, force, debug, verbose);

        case command_t::dump_text:
            return ::do_dump(input_files[0], false);

        case command_t::dump_binary:
            return ::load_reference_files(reference_files, verbose) ? ::do_dump(input_files[0], true) : ::EXIT_CODE_BAD_REFERENCE;

        default:
            return ::show_usage();
    }
}
