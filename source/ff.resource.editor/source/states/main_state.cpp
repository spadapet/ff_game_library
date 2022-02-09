#include "pch.h"
#include "source/states/main_state.h"
#include "source/ui/main_window.xaml.h"

editor::main_state::main_state()
    : main_ui(Noesis::MakePtr<editor::main_window>())
    , main_ui_state(std::make_shared<ff::ui_view_state>(std::make_shared<ff::ui_view>(this->main_ui, ff::ui_view_options::cache_render)))
{
    this->main_ui_state->view()->size(ff::app_render_target());
}

size_t editor::main_state::child_state_count()
{
    return 1;
}

ff::state* editor::main_state::child_state(size_t index)
{
    return this->main_ui_state.get();
}
