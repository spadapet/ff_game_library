#pragma once

#include "../base/constants.h"
#include "../base/string.h"

namespace ff::internal::log
{
    void write(std::string_view text);
    void write_debug(std::string_view text);
}

namespace ff::log
{
    void file(std::ostream* file_stream);

    enum class type : size_t
    {
        none, // not visible by default
        normal, // generic use
        debug, // debug build only

        // FF libraries
        application,
        audio,
        base_memory,
        data,
        dx12,
        dx12_fence,
        dx12_residency,
        dx12_target,
        dxgi,
        graphics,
        input,
        resource,
        resource_load,
        test,
        ui,
        ui_focus,
        ui_mem,
    };

    std::vector<ff::log::type> types();
    ff::log::type register_type(std::string_view name, bool enabled);
    ff::log::type lookup_type(std::string_view name);
    std::string_view type_name(ff::log::type type);
    bool type_enabled(ff::log::type type);
    void type_enabled(ff::log::type type, bool value);

    template<class... Args>
    void write(ff::log::type type, Args&&... args)
    {
        if (ff::log::type_enabled(type))
        {
            std::ostringstream ostr;
            ostr << "[" << ff::log::type_name(type) << "," << ff::string::time() << "] ";
            (ostr << ... << args);
            ostr << "\r\n";

            std::string str = ostr.str();
            ff::internal::log::write(str);
            ff::internal::log::write_debug(str);
        }
    }

    template<class... Args>
    void write_debug_fail(ff::log::type type, Args&&... args)
    {
        ff::log::write(type, std::forward<Args>(args)...);

        if constexpr (ff::constants::debug_build)
        {
            __debugbreak();
        }
    }
}
