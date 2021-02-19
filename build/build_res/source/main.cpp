#include "pch.h"

static std::vector<std::string> get_command_line()
{
    std::vector<std::string> tokens;
    std::wstring token;
    wchar_t quote = 0;

    for (const wchar_t* sz = ::GetCommandLine(); sz && *sz; sz++)
    {
        if (!quote)
        {
            if (::iswspace(*sz))
            {
                // end of a token
                if (!token.empty())
                {
                    tokens.push_back(ff::string::to_string(token));
                    token.clear();
                }
            }
            else if (*sz == '\"' || *sz == '\'')
            {
                // start of a string
                quote = *sz;
            }
            else
            {
                token += *sz;
            }
        }
        else
        {
            // inside of a quoted string

            if (*sz == quote)
            {
                // the string has ended
                quote = 0;
            }
            else
            {
                token += *sz;
            }
        }
    }

    if (!token.empty())
    {
        // save the last token

        tokens.push_back(ff::string::to_string(token));
        token.clear();
    }

    return tokens;
}

static void ShowUsage()
{
    std::cerr << "Resource packer usage:" << std::endl;
    std::cerr << "    respack.exe -in \"input file\" [-out \"output file\"] [-ref \"types.dll\"] [-debug] [-force]" << std::endl;
    std::cerr << "    respack.exe -dump \"pack file\"" << std::endl;
}

static bool test_load_resources(const ff::dict& dict)
{
#ifdef _DEBUG
    dict.debug_print();

    ff::ComPtr<ff::IResources> resources;
    assertRetVal(ff::CreateResources(nullptr, dict, &resources), false);

    ff::Vector<ff::AutoResourceValue> values;
    ff::Vector<ff::String> names = dict.GetAllNames();

    for (ff::StringRef name : names)
    {
        if (name != L"Values" && std::wcsncmp(name.c_str(), L"res:", 4))
        {
            values.Push(ff::AutoResourceValue(resources, name));
        }
    }

    for (ff::AutoResourceValue& value : values)
    {
        value.Flush();
    }

    for (ff::AutoResourceValue& value : values)
    {
        if (!value.GetValue() || value.GetValue()->IsType<ff::NullValue>())
        {
            std::wcerr << L"Failed to create resource object: " << value.GetName() << std::endl;
            assertRetVal(false, false);
        }
    }
#endif

    return true;
}

static bool CompileResourcePack(ff::StringRef inputFile, ff::StringRef outputFile, bool debug)
{
    ff::Vector<ff::String> errors;
    ff::dict dict = ff::LoadResourcesFromFile(inputFile, debug, errors);

    if (errors.Size())
    {
        for (ff::String error : errors)
        {
            std::wcerr << error << std::endl;
        }

        assertRetVal(false, false);
    }

    assertRetVal(::test_load_resources(dict), false);
    assertRetVal(ff::SaveResourceCacheToFile(dict, outputFile), false);

    return true;
}

class SaveTextureVisitor : public ff::DictVisitorBase
{
public:
    SaveTextureVisitor()
        : _path(ff::GetTempDirectory())
    {
        verify(_globals.Startup(ff::AppGlobalsFlags::GraphicsAndAudio));

        ff::AppendPathTail(_path, ff::String(L"ResPackDump"));
        verify(ff::CreateDirectory(_path));
    }

    ~SaveTextureVisitor()
    {
        if (ff::DirectoryExists(_path))
        {
            ::ShellExecute(nullptr, L"open", _path.c_str(), nullptr, _path.c_str(), SW_SHOWDEFAULT);
        }
    }

protected:
    virtual ff::ValuePtr TransformDict(const ff::dict& dict) override
    {
        ff::ValuePtrT<ff::StringValue> typeValue = dict.GetValue(ff::RES_TYPE);
        if (typeValue)
        {
            const ff::ModuleClassInfo* typeInfo = ff::ProcessGlobals::Get()->GetModules().FindClassInfo(typeValue.GetValue());
            if (typeInfo)
            {
                ff::String name(L"value");
                ff::dict resourceDict;
                resourceDict.Set<ff::DictValue>(name, ff::dict(dict));

                ff::ComPtr<ff::IResources> resources;
                ff::ComPtr<ff::IResourceSaveFile> saver;
                if (ff::CreateResources(&_globals, resourceDict, &resources) &&
                    resources->FlushResource(resources->GetResource(name))->QueryObject(__uuidof(ff::IResourceSaveFile), (void**)&saver))
                {
                    ff::String file = _path;
                    ff::AppendPathTail(file, ff::CleanFileName(GetPath()) + saver->GetFileExtension());

                    std::wcout << L"Saving: " << file.c_str() << std::endl;

                    if (!saver->SaveToFile(file))
                    {
                        AddError(ff::String::format_new(L"Failed to save resource to file: %s", file.c_str()));
                    }
                }
            }
        }

        return ff::DictVisitorBase::TransformDict(dict);
    }

private:
    ff::String _path;
    ff::DesktopGlobals _globals;
};

static int DumpFile(ff::StringRef dumpFile, bool dumpBin)
{
    if (!ff::FileExists(dumpFile))
    {
        std::wcerr << L"File doesn't exist: " << dumpFile << std::endl;
        return 1;
    }

    ff::ComPtr<ff::IData> dumpData;
    if (!ff::ReadWholeFileMemMapped(dumpFile, &dumpData))
    {
        std::wcerr << L"Can't read file: " << dumpFile << std::endl;
        return 2;
    }

    ff::dict dumpDict;
    ff::ComPtr<ff::IDataReader> dumpReader;
    if (!ff::CreateDataReader(dumpData, 0, &dumpReader) || !ff::LoadDict(dumpReader, dumpDict))
    {
        std::wcerr << L"Can't load file: " << dumpFile << std::endl;
        return 3;
    }

    ff::Log log;
    log.SetConsoleOutput(true);

    ff::DumpDict(dumpFile, dumpDict, &log, false);

    if (dumpBin)
    {
        SaveTextureVisitor visitor;
        ff::Vector<ff::String> errors;
        visitor.VisitDict(dumpDict, errors);

        if (errors.Size())
        {
            for (ff::StringRef error : errors)
            {
                std::wcerr << error.c_str() << std::endl;
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
    ff::StaticString parseTypes(L".parsetypes.dll");

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
                    std::wcout << L"ResPack: Loaded: " << file << std::endl;
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
    ff::String inputFile;
    ff::String outputFile;
    ff::String dumpFile;
    bool debug = false;
    bool force = false;
    bool verbose = false;
    bool dumpBin = false;

    for (size_t i = 1; i < args.Size(); i++)
    {
        ff::StringRef arg = args[i];

        if (arg == L"-in" && i + 1 < args.Size())
        {
            inputFile = ff::GetCurrentDirectory();
            ff::AppendPathTail(inputFile, args[++i]);
        }
        else if (arg == L"-out" && i + 1 < args.Size())
        {
            outputFile = ff::GetCurrentDirectory();
            ff::AppendPathTail(outputFile, args[++i]);
        }
        else if (arg == L"-ref" && i + 1 < args.Size())
        {
            ff::String file = ff::GetCurrentDirectory();
            ff::AppendPathTail(file, args[++i]);
            refs.Push(file);
        }
        else if ((arg == L"-dump" || arg == L"-dumpbin") && i + 1 < args.Size())
        {
            dumpBin = (arg == L"-dumpbin");
            dumpFile = ff::GetCurrentDirectory();
            ff::AppendPathTail(dumpFile, args[++i]);
        }
        else if (arg == L"-debug")
        {
            debug = true;
        }
        else if (arg == L"-force")
        {
            force = true;
        }
        else if (arg == L"-verbose")
        {
            verbose = true;
        }
        else
        {
            ShowUsage();
            return 1;
        }
    }

    if (dumpFile.size())
    {
        if (inputFile.size() || outputFile.size())
        {
            ShowUsage();
            return 1;
        }

        return DumpFile(dumpFile, dumpBin);
    }

    if (inputFile.empty())
    {
        ShowUsage();
        return 1;
    }

    if (outputFile.empty())
    {
        outputFile = inputFile;
        ff::ChangePathExtension(outputFile, ff::String(L"pack"));
    }

    if (!ff::FileExists(inputFile))
    {
        std::wcerr << L"ResPack: File doesn't exist: " << inputFile << std::endl;
        return 2;
    }

    bool skipped = !force && ff::IsResourceCacheUpToDate(inputFile, outputFile, debug);
    std::wcout << inputFile << L" -> " << outputFile << (skipped ? L" (skipped)" : L"") << std::endl;

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
                std::wcout << L"ResPack: Loaded: " << file << std::endl;
            }
        }
        else
        {
            std::wcerr << L"ResPack: Reference file doesn't exist: " << file << std::endl;
            return 3;
        }
    }

    ff::Timer timer;
    ff::DesktopGlobals desktopGlobals;
    if (!desktopGlobals.Startup(ff::AppGlobalsFlags::GraphicsAndAudio))
    {
        std::wcerr << L"ResPack: Failed to initialize app globals" << std::endl;
        return 4;
    }

    if (!::CompileResourcePack(inputFile, outputFile, debug))
    {
        std::wcerr << L"ResPack: FAILED" << std::endl;
        return 5;
    }

    if (verbose)
    {
        std::wcout <<
            L"ResPack: Time: " <<
            std::fixed <<
            std::setprecision(3) <<
            timer.Tick() <<
            L"s (" << inputFile << L")" <<
            std::endl;
    }

    return 0;
}
