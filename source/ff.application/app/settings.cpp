#include "pch.h"
#include "app/app.h"
#include "app/settings.h"

static std::recursive_mutex mutex;
static ff::dict named_settings;
static bool settings_changed;
static ff::signal<> save_settings_signal;

static std::filesystem::path settings_path()
{
    std::ostringstream name;
    name << "settings_" << ff::constants::bits_build << ".bin";
    return ff::app_roaming_path() / name.str();
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

    std::filesystem::path settings_path = ::settings_path();
    if (ff::filesystem::exists(settings_path))
    {
        auto data = ff::filesystem::read_binary_file(settings_path);
        ff::data_reader reader(data);
        if (data && ff::dict::load(reader, ::named_settings))
        {
            ff::log::write(ff::log::type::application, "Load settings: ", ff::filesystem::to_string(settings_path), "\r\n", ::named_settings);
        }
        else
        {
            ff::log::write(ff::log::type::application, "Invalid settings: ", ff::filesystem::to_string(settings_path));
        }
    }
    else
    {
        ff::log::write(ff::log::type::application, "No settings file: ", ff::filesystem::to_string(settings_path));
    }

    ::settings_changed = false;
}

bool ff::internal::app::save_settings()
{
    std::scoped_lock lock(::mutex);
    if (::settings_changed)
    {
        std::filesystem::path settings_path = ::settings_path();
        ff::log::write(ff::log::type::application, "Save settings: ", ff::filesystem::to_string(settings_path), "\r\n", ::named_settings);

        ff::file_writer writer(settings_path);
        if (!::named_settings.save(writer))
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
