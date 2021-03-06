#include "pch.h"
#include "app.h"
#include "filesystem.h"
#include "settings.h"

static std::recursive_mutex mutex;
static ff::dict named_settings;
static bool settings_changed;
static ff::signal<> save_settings_signal;

static std::filesystem::path settings_path()
{
    std::ostringstream name;
    name << "settings_" << ff::constants::bits_build << ".bin";
    return ff::filesystem::app_roaming_path() / name.str();
}

void ff::internal::app::clear_settings()
{
    std::scoped_lock lock(::mutex);

    if (!::named_settings.empty())
    {
        ::named_settings.clear();
        ::settings_changed = true;
    }
}

void ff::internal::app::load_settings()
{
    std::scoped_lock lock(::mutex);
    ff::internal::app::clear_settings();

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
        ff::log::write(str.str());
    }
    else
    {
        std::ostringstream str;
        str << "No settings file: " << ff::filesystem::to_string(settings_path);
        ff::log::write(str.str());
    }

    ::settings_changed = false;
}

bool ff::internal::app::save_settings()
{
    std::scoped_lock lock(::mutex);
    if (::settings_changed)
    {
        std::filesystem::path settings_path = ::settings_path();
        std::ostringstream str;
        str << "App save settings: " << ff::filesystem::to_string(settings_path) << std::endl;
        ::named_settings.print(str);
        ff::log::write(str.str());

        if (!::named_settings.save(ff::file_writer(settings_path)))
        {
            assert(false);
            return false;
        }

        ::settings_changed = false;
    }

    return true;
}

void ff::internal::app::request_save_settings()
{
    ::save_settings_signal.notify();
}

ff::dict ff::settings(std::string_view name)
{
    std::scoped_lock lock(::mutex);
    return ::named_settings.get<ff::dict>(name);
}

void ff::settings(std::string_view name, const ff::dict& dict)
{
    std::scoped_lock lock(::mutex);

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

ff::signal_sink<>& ff::request_save_settings_sink()
{
    return ::save_settings_signal;
}
