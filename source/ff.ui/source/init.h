#pragma once

namespace ff
{
    struct init_ui_params
    {
        std::function<const ff::dxgi::palette_base* ()> palette_func;
        std::function<void()> register_components_func;
        std::function<Noesis::Ptr<Noesis::ResourceDictionary>(std::string_view)> create_application_resources_func;
        std::function<void(Noesis::ResourceDictionary*)> application_resources_loaded_func;

        std::string noesis_license_name;
        std::string noesis_license_key;
        std::string application_resources_name;
    };

    class init_ui
    {
    public:
        init_ui(const ff::init_ui_params& ui_params);
        ~init_ui();

        operator bool() const;

    private:
        ff::init_input init_input;
        ff::init_graphics init_graphics;
    };
}
