#pragma once

namespace editor
{
    class dialog : public Noesis::UserControl
    {
    public:
        dialog();

    private:
        void initialized(Noesis::BaseComponent* sender, const Noesis::EventArgs& args);

        NS_DECLARE_REFLECTION(editor::dialog, Noesis::UserControl);
    };
}
