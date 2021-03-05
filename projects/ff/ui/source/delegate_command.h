#pragma once

namespace ff::ui
{
    class delegate_command : public Noesis::BaseCommand
    {
    public:
        using execute_func = typename std::function<void(Noesis::BaseComponent*)>;
        using can_execute_func = typename std::function<bool(Noesis::BaseComponent*)>;

        delegate_command() = default;
        delegate_command(execute_func&& execute);
        delegate_command(can_execute_func&& canExecute, execute_func&& execute);

        bool CanExecute(Noesis::BaseComponent* param) const override;
        void Execute(Noesis::BaseComponent* param) const override;

    private:
        can_execute_func can_execute;
        execute_func execute;

        NS_DECLARE_REFLECTION(delegate_command, BaseCommand)
    };
}
