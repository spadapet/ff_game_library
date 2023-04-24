#include "pch.h"
#include "source/models/project_vm.h"

NS_IMPLEMENT_REFLECTION(editor::project_vm, "editor.project_vm")
{
    NsProp("dirty", &editor::project_vm::dirty);
    NsProp("file_name", &editor::project_vm::file_name);
    NsProp("file_name_raw", &editor::project_vm::file_name);
    NsProp("full_path", &editor::project_vm::full_path);
    NsProp("full_path_raw", &editor::project_vm::full_path);
}

editor::project_vm::project_vm()
    : project_vm(std::filesystem::path())
{}

editor::project_vm::project_vm(const std::filesystem::path& path)
    : path_(ff::filesystem::to_string(path))
    , name_(ff::filesystem::to_string(path.filename()))
    , dirty_(false)
{}

bool editor::project_vm::dirty() const
{
    return this->dirty_;
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
    std::wstring path = ff::string::to_wstring(std::string_view(this->full_path_raw()));

    if (!save_as && !path.empty() && ff::filesystem::exists(path) && co_await this->save_async(path))
    {
        co_return true;
    }

    co_await ff::task::resume_on_main();

    wchar_t buffer[1024]{};
    ::wcscpy_s(buffer, path.c_str());

    ::OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = *ff::window::main();
    ofn.hInstance = ff::get_hinstance();
    ofn.lpstrDefExt = L"proj.json";
    ofn.lpstrFilter = L"Project Files (*.proj.json)\0*.proj.json\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (::GetSaveFileName(&ofn))
    {
        path = ofn.lpstrFile;
        co_await ff::task::resume_on_game();
        co_return this->save_async(path);
    }

    co_return false;
}

ff::co_task<bool> editor::project_vm::save_async(const std::filesystem::path& path)
{
    std::vector<ff::value_ptr> sources;

    for (int i = 0; i < this->sources.Count(); i++)
    {
        ff::dict dict = this->sources.Get(i)->save();
        sources.push_back(ff::value::create<ff::dict>(std::move(dict)));
    }

    ff::dict dict;
    dict.set<std::vector<ff::value_ptr>>("sources", std::move(sources));

    std::ostringstream text_stream;
    ff::json_write(dict, text_stream);

    if (ff::filesystem::write_text_file(path, text_stream.str()))
    {
        this->path_ = ff::filesystem::to_string(path);
        this->name_ = ff::filesystem::to_string(path.filename());
        this->dirty_ = false;

        this->property_changed("full_path");
        this->property_changed("file_name");
        this->property_changed("dirty");

        co_return true;
    }

    co_return false;
}
