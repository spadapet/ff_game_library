#pragma once

namespace editor
{
    class dialog_content_base;
    class project_vm;

    class main_vm : public ff::ui::notify_property_changed_base
    {
    public:
        main_vm();
        virtual ~main_vm() override;

        static editor::main_vm* get();
        editor::project_vm* project() const;
        void project(editor::project_vm* project_);

        bool has_modal_dialog() const;
        void push_modal_dialog(editor::dialog_content_base* dialog);
        bool remove_modal_dialog(editor::dialog_content_base* dialog, int result);
        editor::dialog_content_base* modal_dialog() const;

    private:
        void file_new_command(Noesis::BaseComponent* param);
        void file_open_command(Noesis::BaseComponent* param);
        void file_save_command(Noesis::BaseComponent* param);
        void file_save_as_command(Noesis::BaseComponent* param);
        void file_exit_command(Noesis::BaseComponent* param);
        void sources_add_command(Noesis::BaseComponent* param);

        Noesis::Ptr<Noesis::BaseCommand> file_new_command_;
        Noesis::Ptr<Noesis::BaseCommand> file_open_command_;
        Noesis::Ptr<Noesis::BaseCommand> file_save_command_;
        Noesis::Ptr<Noesis::BaseCommand> file_save_as_command_;
        Noesis::Ptr<Noesis::BaseCommand> file_exit_command_;
        Noesis::Ptr<Noesis::BaseCommand> sources_add_command_;
        Noesis::Ptr<Noesis::BaseCommand> ok_dialog_command_;
        Noesis::Ptr<Noesis::BaseCommand> cancel_dialog_command_;

        Noesis::Ptr<editor::project_vm> project_;
        std::vector<Noesis::Ptr<editor::dialog_content_base>> modal_dialogs;

        NS_DECLARE_REFLECTION(editor::main_vm, ff::ui::notify_property_changed_base);
    };
}
