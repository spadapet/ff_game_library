#pragma once

#include "../graphics/sprite.h"

namespace ff
{
    class sprite_list : public ff::resource_object_base
    {
    public:
        sprite_list(std::vector<ff::sprite>&& sprites);
        sprite_list(sprite_list&& other) noexcept = default;
        sprite_list(const sprite_list& other) noexcept = default;

        sprite_list& operator=(sprite_list&& other) noexcept = default;
        sprite_list& operator=(const sprite_list & other) noexcept = default;

        size_t size() const;
        const ff::sprite* get(size_t index) const;
        const ff::sprite* get(std::string_view name) const;

        // resource_object_base
        virtual ff::dict resource_get_siblings(const std::shared_ptr<resource>& self) const;

    protected:
        virtual bool save_to_cache(ff::dict& dict) const override;

    private:
        std::vector<ff::sprite> sprites;
        std::unordered_map<std::string_view, const ff::sprite*> name_to_sprite;
    };
}

namespace ff::internal
{
    class sprite_list_factory : public ff::resource_object_factory<sprite_list>
    {
    public:
        using ff::resource_object_factory<sprite_list>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
