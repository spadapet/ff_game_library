#pragma once

namespace editor
{
    class main_vm : public ff::ui::notify_propety_changed_base
    {
    public:
        main_vm();

    private:
        void file_exit_command(Noesis::BaseComponent* param);

        Noesis::Ptr<Noesis::ICommand> file_exit_command_;

        NS_DECLARE_REFLECTION(editor::main_vm, ff::ui::notify_propety_changed_base);
    };
}
