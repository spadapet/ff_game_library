#include "pch.h"
#include "source/models/project_vm.h"

NS_IMPLEMENT_REFLECTION(editor::project_vm, "editor.project_vm")
{
    NsProp("dirty", &editor::project_vm::dirty);
    NsProp("file_name", &editor::project_vm::file_name);
    NsProp("file_name_raw", &editor::project_vm::file_name);
    NsProp("full_path", &editor::project_vm::full_path);
    NsProp("full_path_raw", &editor::project_vm::full_path);
    NsProp("plugins", &editor::project_vm::plugins);
    NsProp("sources", &editor::project_vm::sources);
}

editor::project_vm::project_vm()
    : project_vm(std::filesystem::path())
{}

editor::project_vm::project_vm(const std::filesystem::path& path)
    : plugins(Noesis::MakePtr<Noesis::ObservableCollection<editor::plugin_vm>>())
    , sources(Noesis::MakePtr<Noesis::ObservableCollection<editor::source_vm>>())
    , path_(ff::filesystem::to_string(path))
    , name_(ff::filesystem::to_string(path.filename()))
{}

bool editor::project_vm::dirty() const
{
    return this->dirty_;
}

void editor::project_vm::dirty(bool value)
{
    this->set_property(this->dirty_, value, "dirty");
}

const char* editor::project_vm::full_path() const
{
    if (this->path_.empty())
    {
        return "Unsaved";
    }

    return this->full_path_raw();
}

const char* editor::project_vm::full_path_raw() const
{
    return this->path_.c_str();
}

const char* editor::project_vm::file_name() const
{
    if (this->name_.empty())
    {
        return "Untitled";
    }

    return this->file_name_raw();
}

const char* editor::project_vm::file_name_raw() const
{
    return this->name_.c_str();
}

ff::co_task<bool> editor::project_vm::save_async(bool save_as)
{
    std::wstring path = ff::string::to_wstring(this->path_);

    if (!save_as && !path.empty() && ff::filesystem::exists(path) && co_await this->save_async(path))
    {
        co_return true;
    }

    co_await ff::task::resume_on_main();

    Microsoft::WRL::ComPtr<IFileSaveDialog> dialog;
    if (SUCCEEDED(::CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
    {
        const COMDLG_FILTERSPEC filters[] = { { L"Project Files  (*.proj.json)", L"*.proj.json" } };
        dialog->SetFileTypes(1, filters);
        dialog->SetDefaultExtension(L"proj.json");
        dialog->SetOptions(FOS_OVERWRITEPROMPT | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dialog->SetFileName(path.c_str());

        if (SUCCEEDED(dialog->Show(*ff::window::main())))
        {
            Microsoft::WRL::ComPtr<IShellItem> item;
            wchar_t* path_str{};

            if (SUCCEEDED(dialog->GetResult(&item)) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path_str)))
            {
                path = path_str;
                ::CoTaskMemFree(path_str);
                co_await ff::task::resume_on_game();
                co_return this->save_async(path);
            }
        }
    }

    co_return false;
}

ff::co_task<bool> editor::project_vm::save_async(const std::filesystem::path& path)
{
    ff::dict dict;

    // Plugins and sources
    {
        std::vector<ff::value_ptr> plugins;
        std::vector<ff::value_ptr> sources;

        for (int i = 0; i < this->plugins->Count(); i++)
        {
            ff::dict dict = this->plugins->Get(i)->save();
            plugins.push_back(ff::value::create<ff::dict>(std::move(dict)));
        }

        for (int i = 0; i < this->sources->Count(); i++)
        {
            ff::dict dict = this->sources->Get(i)->save();
            sources.push_back(ff::value::create<ff::dict>(std::move(dict)));
        }

        dict.set<std::vector<ff::value_ptr>>("plugins", std::move(plugins));
        dict.set<std::vector<ff::value_ptr>>("sources", std::move(sources));
    }

    std::ostringstream text_stream;
    ff::json_write(dict, text_stream);

    if (ff::filesystem::write_text_file(path, text_stream.str()))
    {
        this->path_ = ff::filesystem::to_string(path);
        this->name_ = ff::filesystem::to_string(path.filename());

        this->properties_changed("full_path", "full_path_raw", "file_name", "file_name_raw");
        this->dirty(false);

        co_return true;
    }

    co_return false;
}

Noesis::Ptr<editor::plugin_vm> editor::project_vm::add_plugin(const std::filesystem::path& path)
{
    for (int i = 0; i < this->plugins->Count(); i++)
    {
        Noesis::Ptr<editor::plugin_vm> plugin(this->plugins->Get(i));
        if (ff::filesystem::equivalent(plugin->path(), path))
        {
            return plugin;
        }
    }

    auto plugin = Noesis::MakePtr<editor::plugin_vm>(path);
    this->plugins->Add(plugin);
    this->dirty(true);
    return plugin;
}

bool editor::project_vm::remove_plugin(editor::plugin_vm* plugin)
{
    for (int i = 0; i < this->plugins->Count(); i++)
    {
        if (this->plugins->Get(i) == plugin)
        {
            this->plugins->RemoveAt(i);
            this->dirty(true);
            return true;
        }
    }

    return false;
}

Noesis::Ptr<editor::source_vm> editor::project_vm::add_source(const std::filesystem::path& path)
{
    for (int i = 0; i < this->sources->Count(); i++)
    {
        Noesis::Ptr<editor::source_vm> source(this->sources->Get(i));
        if (ff::filesystem::equivalent(source->path(), path))
        {
            return source;
        }
    }

    auto source = Noesis::MakePtr<editor::source_vm>(path);
    this->sources->Add(source);
    this->dirty(true);
    return source;
}

bool editor::project_vm::remove_source(editor::source_vm* source)
{
    for (int i = 0; i < this->sources->Count(); i++)
    {
        if (this->sources->Get(i) == source)
        {
            this->sources->RemoveAt(i);
            this->dirty(true);
            return true;
        }
    }

    return false;
}
