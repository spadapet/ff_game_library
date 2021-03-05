#include "pch.h"
#include "delegate_command.h"

NS_IMPLEMENT_REFLECTION(ff::ui::delegate_command)
{}

ff::ui::delegate_command::delegate_command(execute_func&& execute)
    : execute(std::move(execute))
{}

ff::ui::delegate_command::delegate_command(can_execute_func&& canExecute, execute_func&& execute)
    : can_execute(std::move(canExecute))
    , execute(std::move(execute))
{}

bool ff::ui::delegate_command::CanExecute(Noesis::BaseComponent* param) const
{
    return !this->can_execute || this->can_execute(param);
}

void ff::ui::delegate_command::Execute(Noesis::BaseComponent* param) const
{
    if (this->execute)
    {
        this->execute(param);
    }
}
