﻿#include "pch.h"

static std::vector<std::string> get_command_line()
{
    std::wstring_view command_line(::GetCommandLine());
    return ff::string::split_command_line(ff::string::to_string(command_line));
}

static void show_usage()
{
    std::cerr << "Command line options:" << std::endl;
    std::cerr << "  1) ff.build_res.exe -in \"input file\" [-out \"output file\"] [-header \"output C++\"] [-ref \"types.dll\"] [-debug] [-force]" << std::endl;
    std::cerr << "  2) ff.build_res.exe -dump \"pack file\"" << std::endl;
    std::cerr << "  3) ff.build_res.exe -dumpbin \"pack file\"" << std::endl;
    std::cerr << std::endl;
    std::cerr << "NOTES:" << std::endl;
    std::cerr << "  With -ref, the reference DLL must contain an exported C method: 'void ff_init()'." << std::endl;
    std::cerr << "  Using -dumpbin will save all binary resources to a temp folder and open it." << std::endl;
}

static bool test_load_resources(const ff::dict& dict)
{
#ifdef _DEBUG
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
        if (!value.valid() || value.value()->is_type<nullptr_t>())
        {
            std::cerr << "Failed to create resource object: " << value.resource()->name() << std::endl;
            assert(false);
            return false;
        }
    }
#endif

    return true;
}

static bool write_header(const std::vector<uint8_t>& data, std::ostream& output, std::string_view cpp_namespace)
{
    const size_t bytes_per_line = 64;
    const char hex_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    output << "namespace { namespace " << cpp_namespace << R"(
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
} }
)";

    return true;
}

static bool compile_resource_pack(
    const std::filesystem::path& input_file,
    const std::filesystem::path& output_file,
    const std::filesystem::path& header_file,
    bool debug)
{
    ff::load_resources_result result = ff::load_resources_from_file(input_file, false, debug);
    if (!result.status || !::test_load_resources(result.dict))
    {
        for (auto& error : result.errors)
        {
            std::cerr << error << std::endl;
        }

        assert(false);
        return false;
    }

    auto data = std::make_shared<std::vector<uint8_t>>();
    data->reserve(1 << 22); // 4MB buffer
    {
        ff::data_writer data_writer(data);
        if (!result.dict.save(data_writer))
        {
            assert(false);
            return false;
        }
    }

    if (!output_file.empty())
    {
        ff::file_writer writer(output_file);
        if (!writer || writer.write(data->data(), data->size()) != data->size())
        {
            assert(false);
            return false;
        }
    }

    if (!header_file.empty())
    {
        std::ofstream header_stream(header_file);
        if (!header_stream || !::write_header(*data, header_stream, result.namespace_))
        {
            assert(false);
            return false;
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
        std::error_code ec;
        std::filesystem::create_directories(this->root_path, ec);
    }

    ~save_to_file_visitor()
    {
        std::error_code ec;
        if (std::filesystem::exists(this->root_path))
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

    std::error_code ec;
    if (!std::filesystem::exists(dump_source_file))
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
    if (!ff::dict::load(reader, dict) || !dict.load_child_dicts())
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
        std::cerr << "ff.build_res: Failed to initialize" << std::endl;
        return 5;
    }

    std::vector<std::filesystem::path> refs;
    std::filesystem::path input_file;
    std::filesystem::path output_file;
    std::filesystem::path header_file;
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
        else if (arg == "-out" && i + 1 < args.size())
        {
            output_file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
        }
        else if (arg == "-header" && i + 1 < args.size())
        {
            header_file = std::filesystem::current_path() / ff::filesystem::to_path(args[++i]);
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
        if (!input_file.empty() || !output_file.empty())
        {
            ::show_usage();
            return 1;
        }

        return ::dump_file(dump_source_file, dump_bin);
    }

    if (input_file.empty())
    {
        ::show_usage();
        return 1;
    }

    if (output_file.empty())
    {
        output_file = input_file;
        output_file.replace_extension(".pack");
    }

    std::error_code ec;
    if (!std::filesystem::exists(input_file, ec))
    {
        std::cerr << "ff.build_res: File doesn't exist: " << input_file << std::endl;
        return 2;
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
            std::cerr << "ff.build_res: Failed to initialize" << std::endl;
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
                    std::cout << "ff.build_res: Loaded: " << ref << std::endl;
                }

                if (!init_func)
                {
                    std::cerr << "ff.build_res: Reference doesn't contain 'ff_init' export: " << ref << std::endl;
                    return 3;
                }

                init_func();
            }
            else
            {
                std::cerr << "ff.build_res: Failed to load reference: " << ref << std::endl;
                return 4;
            }
        }

        if (!::compile_resource_pack(input_file, output_file, header_file, debug))
        {
            std::cerr << "ff.build_res: Compile failed" << std::endl;
            return 6;
        }
    }

    if (verbose)
    {
        std::cout <<
            "ff.build_res: Time: " <<
            std::fixed <<
            std::setprecision(3) <<
            timer.tick() <<
            "s (" << input_file << ")" <<
            std::endl;
    }

    return 0;
}
