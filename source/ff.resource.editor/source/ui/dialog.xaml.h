#pragma once

namespace editor
{
    class dialog : public Noesis::UserControl
    {
    public:
        dialog();

    private:
        NS_DECLARE_REFLECTION(editor::dialog, Noesis::UserControl);
    };
}
