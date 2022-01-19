#include "pch.h"
#include "assert.h"
#include "log.h"
#include "stable_hash.h"
#include "string.h"

namespace
{
    struct log_type
    {
        std::string name;
        bool enabled;
    };
}

static std::ostream* file_stream;

static std::unordered_map<ff::log::type, ::log_type> types
{
    { ff::log::type::none, { "ff", false } },
    { ff::log::type::normal, { "ff/game", true } },
    { ff::log::type::debug, { "ff/debug", DEBUG } },

    { ff::log::type::application, { "ff/app", true } },
    { ff::log::type::audio, { "ff/audio", true } },
    { ff::log::type::base_memory, { "ff/mem", true } },
    { ff::log::type::data, { "ff/data", true } },
    { ff::log::type::dx12, { "ff/dx12", true } },
    { ff::log::type::dx12_fence, { "ff/dx12_fence", false } },
    { ff::log::type::dx12_residency, { "ff/dx12_residency", DEBUG } },
    { ff::log::type::dxgi, { "ff/dxgi", true } },
    { ff::log::type::graphics, { "ff/graph", true } },
    { ff::log::type::input, { "ff/input", true } },
    { ff::log::type::resource, { "ff/res", true } },
    { ff::log::type::resource_load, { "ff/res_load", false } },
    { ff::log::type::test, { "ff/test", true } },
    { ff::log::type::ui, { "ff/ui", true } },
};

void ff::internal::log::write(std::string_view text)
{
    if (::file_stream)
    {
        *::file_stream << text;
    }

#ifdef _DEBUG
    std::cerr << text;
#endif
}

void ff::internal::log::write_debug(std::string_view text)
{
#ifdef _DEBUG
    ::OutputDebugString(ff::string::to_wstring(text).c_str());
#endif
}

void ff::log::file(std::ostream* file_stream)
{
    ::file_stream = file_stream;
}

std::vector<ff::log::type> ff::log::types()
{
    std::vector<ff::log::type> types;
    types.reserve(::types.size());

    for (auto& i : ::types)
    {
        types.push_back(i.first);
    }

    return types;
}

ff::log::type ff::log::register_type(std::string_view name, bool enabled)
{
    ff::log::type type = static_cast<ff::log::type>(ff::stable_hash_func(name));
    auto i = ::types.try_emplace(type, ::log_type{ std::string(name), enabled });
    assert_msg(i.second, "Log name already registered");

    return type;
}

ff::log::type ff::log::lookup_type(std::string_view name)
{
    for (auto& i : ::types)
    {
        if (i.second.name == name)
        {
            return i.first;
        }
    }

    debug_fail_msg("Invalid log type name");
    return ff::log::type::none;
}

std::string_view ff::log::type_name(ff::log::type type)
{
    auto i = ::types.find(type);
    assert_msg_ret_val(i != ::types.end(), "Invalid log type", "");
    return i->second.name;
}

bool ff::log::type_enabled(ff::log::type type)
{
    auto i = ::types.find(type);
    assert_msg_ret_val(i != ::types.end(), "Invalid log type", false);
    return i->second.enabled;
}

void ff::log::type_enabled(ff::log::type type, bool value)
{
    auto i = ::types.find(type);
    assert_msg_ret(i != ::types.end(), "Invalid log type");
    i->second.enabled = value;
}
