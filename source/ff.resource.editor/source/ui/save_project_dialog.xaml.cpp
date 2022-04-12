#include "pch.h"
#include "source/models/main_vm.h"
#include "source/models/project_vm.h"
#include "source/ui/save_project_dialog.xaml.h"

NS_IMPLEMENT_REFLECTION(editor::save_project_dialog, "editor.save_project_dialog")
{
    NsProp("project", &editor::save_project_dialog::project);
}

editor::save_project_dialog::save_project_dialog()
    : project_(editor::main_vm::get()->project())
{
    Noesis::GUI::LoadComponent(this, "save_project_dialog.xaml");
}

editor::project_vm* editor::save_project_dialog::project() const
{
    return this->project_;
}

bool editor::save_project_dialog::can_window_close()
{
    return false;
}

bool editor::save_project_dialog::has_close_command(int result)
{
    return true;
}

bool editor::save_project_dialog::apply_changes(int result)
{
    if (result == editor::dialog_content_base::RESULT_OK)
    {
        wchar_t buffer[1024]{};

        ::OPENFILENAME ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = *ff::window::main();
        ofn.hInstance = ff::get_hinstance();
        ofn.lpstrFilter = L"*.proj.json";
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = 1024;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (::GetSaveFileName(&ofn))
        {
        }
    }

    return true;
}
