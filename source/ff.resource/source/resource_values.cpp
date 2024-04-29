#include "pch.h"
#include "resource_values.h"

ff::resource_values::resource_values(const ff::dict& dict)
{
    this->populate_user_languages();
    this->populate_values(dict);
}

ff::value_ptr ff::resource_values::get_resource_value(std::string_view name) const
{
    for (const std::string& lang_name : this->user_langs)
    {
        auto iter = this->lang_to_dict.find(lang_name);
        if (iter != this->lang_to_dict.cend())
        {
            const ff::dict& dict = iter->second;
            ff::value_ptr value = dict.get(name);
            if (value)
            {
                return value;
            }
        }
    }

    return nullptr;
}

std::string ff::resource_values::get_string_resource_value(std::string_view name) const
{
    return this->get_resource_value(name)->convert_or_default<std::string>()->get<std::string>();
}

bool ff::resource_values::save_to_cache(ff::dict& dict) const
{
    dict.set(this->original_dict, false);
    return true;
}

void ff::resource_values::populate_user_languages()
{
    this->user_langs.push_back("override");

    wchar_t full_lang_name[LOCALE_NAME_MAX_LENGTH];
    if (::GetUserDefaultLocaleName(full_lang_name, LOCALE_NAME_MAX_LENGTH))
    {
        std::string lang_name = ff::string::to_string(full_lang_name);
        while (lang_name.size())
        {
            this->user_langs.push_back(lang_name);

            size_t lastDash = lang_name.rfind(L'-');
            if (lastDash == std::string::npos)
            {
                lastDash = 0;
            }

            lang_name = lang_name.substr(0, lastDash);
        }
    }

    this->user_langs.push_back("global");
    this->user_langs.push_back(std::string());
}

void ff::resource_values::populate_values(const ff::dict& dict)
{
    this->original_dict = dict;

    for (std::string_view lang_name : dict.child_names())
    {
        ff::value_ptr lang_dict_value = ff::type::try_get_dict_from_data(dict.get(lang_name));
        if (lang_dict_value)
        {
            ff::dict lang_dict = lang_dict_value->get<ff::dict>();
            lang_dict.load_child_dicts();

            auto iter = this->lang_to_dict.try_emplace(lang_name, ff::dict());
            iter.first->second.set(lang_dict, false);
        }
    }
}

std::shared_ptr<ff::resource_object_base> ff::internal::resource_values_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    return this->load_from_cache(dict);
}

std::shared_ptr<ff::resource_object_base> ff::internal::resource_values_factory::load_from_cache(const ff::dict& dict) const
{
    return std::make_shared<ff::resource_values>(ff::dict(dict));
}
