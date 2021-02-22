#include "pch.h"

static std::vector<std::string> get_command_line()
{
    std::wstring_view command_line(::GetCommandLine());
    return ff::string::split_command_line(ff::string::to_string(command_line));
}

static void show_usage()
{
    std::cerr << "ff.build_res.exe usage:" << std::endl;
    std::cerr << "    respack.exe -in \"input file\" [-out \"output file\"] [-ref \"types.dll\"] [-debug] [-force]" << std::endl;
    std::cerr << "    respack.exe -dump \"pack file\"" << std::endl;
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

static bool compile_resource_pack(const std::filesystem::path& input_file, const std::filesystem::path& output_file, bool debug)
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

    ff::file_writer writer(output_file);
    if (!writer || !result.dict.save(writer))
    {
        assert(false);
        return false;
    }

    return true;
}

class save_texture_visitor : public ff::dict_visitor_base
{
public:
    save_texture_visitor()
        : path(ff::filesystem::temp_directory_path() / "ff.build_res.dump")
    {
        std::error_code ec;
        std::filesystem::create_directories(this->path, ec);
    }

    ~save_texture_visitor()
    {
        std::error_code ec;
        if (std::filesystem::exists(this->path))
        {
            const wchar_t* path_str = this->path.native().c_str();
            ::ShellExecute(nullptr, L"open", path_str, nullptr, path_str, SW_SHOWDEFAULT);
        }
    }

protected:
    virtual ff::value_ptr transform_dict(const ff::dict& dict) override
    {
        ff::value_ptr type_value = dict.get(ff::internal::RES_TYPE)->try_convert<std::string>();
        if (type_value)
        {
            const ff::resource_object_factory_base* factory = ff::resource_object_base::get_factory(type_value->get<std::string>());
            if (factory)
            {
                auto resource_object = factory->load_from_cache(dict);
                if (resource_object)
                {
                    resource_object->resource_save_to_file(this->path, );
                }
            }

            const ff::ModuleClassInfo* typeInfo = ff::ProcessGlobals::Get()->GetModules().FindClassInfo(type_value.GetValue());
            if (typeInfo)
            {
                ff::String name("value");
                ff::dict resourceDict;
                resourceDict.Set<ff::DictValue>(name, ff::dict(dict));

                ff::ComPtr<ff::IResources> resources;
                ff::ComPtr<ff::IResourceSaveFile> saver;
                if (ff::CreateResources(&_globals, resourceDict, &resources) &&
                    resources->FlushResource(resources->GetResource(name))->QueryObject(__uuidof(ff::IResourceSaveFile), (void**)&saver))
                {
                    ff::String file = _path;
                    ff::AppendPathTail(file, ff::CleanFileName(GetPath()) + saver->GetFileExtension());

                    std::wcout << "Saving: " << file.c_str() << std::endl;

                    if (!saver->SaveToFile(file))
                    {
                        AddError(ff::String::format_new("Failed to save resource to file: %s", file.c_str()));
                    }
                }
            }
        }

        return ff::DictVisitorBase::TransformDict(dict);
    }

private:
    std::filesystem::path path;
};

static int DumpFile(ff::StringRef dumpFile, bool dumpBin)
{
    if (!ff::FileExists(dumpFile))
    {
        std::cerr << "File doesn't exist: " << dumpFile << std::endl;
        return 1;
    }

    ff::ComPtr<ff::IData> dumpData;
    if (!ff::ReadWholeFileMemMapped(dumpFile, &dumpData))
    {
        std::cerr << "Can't read file: " << dumpFile << std::endl;
        return 2;
    }

    ff::dict dumpDict;
    ff::ComPtr<ff::IDataReader> dumpReader;
    if (!ff::CreateDataReader(dumpData, 0, &dumpReader) || !ff::LoadDict(dumpReader, dumpDict))
    {
        std::cerr << "Can't load file: " << dumpFile << std::endl;
        return 3;
    }

    ff::Log log;
    log.SetConsoleOutput(true);

    ff::DumpDict(dumpFile, dumpDict, &log, false);

    if (dumpBin)
    {
        save_texture_visitor visitor;
        ff::Vector<ff::String> errors;
        visitor.VisitDict(dumpDict, errors);

        if (errors.Size())
        {
            for (ff::StringRef error : errors)
            {
                std::cerr << error.c_str() << std::endl;
            }

            return 4;
        }
    }

    return 0;
}

static void LoadSiblingParseTypes()
{
    ff::Vector<ff::String> dirs;
    ff::Vector<ff::String> files;
    ff::String dir = ff::GetExecutableDirectory();
    ff::StaticString parseTypes(".parsetypes.dll");

    if (ff::GetDirectoryContents(dir, dirs, files))
    {
        for (ff::String file : files)
        {
            if (file.size() > parseTypes.GetString().size() && !::_wcsnicmp(
                file.data() + file.size() - parseTypes.GetString().size(),
                parseTypes.GetString().c_str(),
                parseTypes.GetString().size()))
            {
                ff::String loadFile = dir;
                ff::AppendPathTail(loadFile, file);

                if (::LoadLibrary(loadFile.c_str()))
                {
                    std::wcout << "ResPack: Loaded: " << file << std::endl;
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    std::unique_ptr<ff::init_input> init_input;
    std::unique_ptr<ff::init_audio> init_audio;
    std::unique_ptr<ff::init_graphics> init_graphics;

    ff::Vector<ff::String> args = ff::TokenizeCommandLine();
    ff::Vector<ff::String> refs;
    ff::String input_file;
    ff::String output_file;
    ff::String dumpFile;
    bool debug = false;
    bool force = false;
    bool verbose = false;
    bool dumpBin = false;

    for (size_t i = 1; i < args.Size(); i++)
    {
        ff::StringRef arg = args[i];

        if (arg == "-in" && i + 1 < args.Size())
        {
            input_file = ff::GetCurrentDirectory();
            ff::AppendPathTail(input_file, args[++i]);
        }
        else if (arg == "-out" && i + 1 < args.Size())
        {
            output_file = ff::GetCurrentDirectory();
            ff::AppendPathTail(output_file, args[++i]);
        }
        else if (arg == "-ref" && i + 1 < args.Size())
        {
            ff::String file = ff::GetCurrentDirectory();
            ff::AppendPathTail(file, args[++i]);
            refs.Push(file);
        }
        else if ((arg == "-dump" || arg == "-dumpbin") && i + 1 < args.Size())
        {
            dumpBin = (arg == "-dumpbin");
            dumpFile = ff::GetCurrentDirectory();
            ff::AppendPathTail(dumpFile, args[++i]);
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
            show_usage();
            return 1;
        }
    }

    if (dumpFile.size())
    {
        if (input_file.size() || output_file.size())
        {
            show_usage();
            return 1;
        }

        return DumpFile(dumpFile, dumpBin);
    }

    if (input_file.empty())
    {
        show_usage();
        return 1;
    }

    if (output_file.empty())
    {
        output_file = input_file;
        ff::ChangePathExtension(output_file, ff::String("pack"));
    }

    if (!ff::FileExists(input_file))
    {
        std::cerr << "ResPack: File doesn't exist: " << input_file << std::endl;
        return 2;
    }

    bool skipped = !force && ff::IsResourceCacheUpToDate(input_file, output_file, debug);
    std::wcout << input_file << " -> " << output_file << (skipped ? " (skipped)" : "") << std::endl;

    if (skipped)
    {
        return 0;
    }

    ::LoadSiblingParseTypes();

    for (ff::StringRef file : refs)
    {
        HMODULE mod = ::LoadLibrary(file.c_str());
        if (mod)
        {
            if (verbose)
            {
                std::wcout << "ResPack: Loaded: " << file << std::endl;
            }
        }
        else
        {
            std::cerr << "ResPack: Reference file doesn't exist: " << file << std::endl;
            return 3;
        }
    }

    ff::Timer timer;
    ff::DesktopGlobals desktopGlobals;
    if (!desktopGlobals.Startup(ff::AppGlobalsFlags::GraphicsAndAudio))
    {
        std::cerr << "ResPack: Failed to initialize app globals" << std::endl;
        return 4;
    }

    if (!::compile_resource_pack(input_file, output_file, debug))
    {
        std::cerr << "ResPack: FAILED" << std::endl;
        return 5;
    }

    if (verbose)
    {
        std::wcout <<
            "ResPack: Time: " <<
            std::fixed <<
            std::setprecision(3) <<
            timer.Tick() <<
            "s (" << input_file << ")" <<
            std::endl;
    }

    return 0;
}
