#include "pch.h"
#include "source/models/main_vm.h"
#include "source/models/project_vm.h"
#include "source/ui/save_project_dialog.xaml.h"
#include "xaml.res.id.h"

NS_IMPLEMENT_REFLECTION(editor::save_project_dialog, "editor.save_project_dialog")
{
    NsProp("project", &editor::save_project_dialog::project);
}

editor::save_project_dialog::save_project_dialog()
    : project_(editor::main_vm::get()->project())
{
    Noesis::GUI::LoadComponent(this, assets::xaml::SAVE_PROJECT_DIALOG_XAML.data());
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

ff::co_task<bool> editor::save_project_dialog::apply_changes_async(int result)
{
    if (result == editor::dialog_content_base::RESULT_OK)
    {
        co_return co_await this->project_->save_async();
    }

    co_return true;
}
