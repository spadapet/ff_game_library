#include "pch.h"
#include "app.h"
#include "settings.h"

static std::recursive_mutex mutex;
static ff::dict named_settings;
static bool settings_changed;

static std::filesystem::path settings_path()
{
    std::ostringstream name;
    name << ff::app_name() << ff::constants::bits_build << "bit settings.bin";
    return ff::filesystem::user_roaming_path() / name.str();
}

static void clear_settings()
{
    std::lock_guard lock(::mutex);

    if (!::named_settings.empty())
    {
        ::named_settings.clear();
        ::settings_changed = true;
    }
}

void ff::internal::app::load_settings()
{
    std::lock_guard lock(::mutex);
    ::clear_settings();

    std::error_code ec;
    std::filesystem::path settings_path = ::settings_path();
    if (std::filesystem::exists(settings_path, ec))
    {
        if (!ff::dict::load(ff::file_reader(settings_path), ::named_settings))
        {
            assert(false);
        }

        std::ostringstream str;
        str << "App load settings: " << ff::filesystem::to_string(settings_path) << std::endl;
        ::named_settings.print(str);
        ff::log::write(str);
    }

    ::settings_changed = false;
}

bool ff::internal::app::save_settings()
{
    std::lock_guard lock(::mutex);
    if (::settings_changed)
    {
        std::filesystem::path settings_path = ::settings_path();
        std::ostringstream str;
        str << "App save settings: " << ff::filesystem::to_string(settings_path) << std::endl;
        ::named_settings.print(str);
        ff::log::write(str);

        if (!::named_settings.save(ff::file_writer(settings_path)))
        {
            assert(false);
            return false;
        }

        ::settings_changed = false;
    }

    return true;
}

ff::dict ff::settings(std::string_view name)
{
    std::lock_guard lock(::mutex);
    return ::named_settings.get<ff::dict>(name);
}

void ff::settings(std::string_view name, const ff::dict& dict)
{
    std::lock_guard lock(::mutex);

    if (dict.empty())
    {
        ::named_settings.set(name, nullptr);
    }
    else
    {
        ::named_settings.set<ff::dict>(name, ff::dict(dict));
    }

    ::settings_changed = true;
}
