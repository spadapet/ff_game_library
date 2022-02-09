#pragma once

namespace editor
{
    class plugin_vm : public ff::ui::notify_propety_changed_base
    {
    public:
        plugin_vm();

    private:
        NS_DECLARE_REFLECTION(editor::plugin_vm, ff::ui::notify_propety_changed_base);
    };
}
