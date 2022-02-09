#pragma once

namespace editor
{
    class shell : public Noesis::UserControl
    {
    public:
        shell();

    private:
        NS_DECLARE_REFLECTION(editor::shell, Noesis::UserControl);
    };
}
