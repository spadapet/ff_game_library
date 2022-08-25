#include "pch.h"
#include "sprite_base.h"
#include "sprite_list.h"
#include "sprite_optimizer.h"
#include "sprite_resource.h"
#include "texture.h"

ff::sprite_list::sprite_list(std::vector<ff::sprite>&& sprites)
    : sprites(std::move(sprites))
{
    this->name_to_sprite.reserve(this->sprites.size());

    for (auto& sprite : this->sprites)
    {
        if (sprite.name().size())
        {
            this->name_to_sprite.try_emplace(sprite.name(), &sprite);
        }
    }
}

size_t ff::sprite_list::size() const
{
    return this->sprites.size();
}

const ff::sprite* ff::sprite_list::get(size_t index) const
{
    return index < this->sprites.size() ? &this->sprites[index] : nullptr;
}

const ff::sprite* ff::sprite_list::get(std::string_view name) const
{
    auto i = this->name_to_sprite.find(name);
    return i != this->name_to_sprite.cend() ? i->second : nullptr;
}

ff::dict ff::sprite_list::resource_get_siblings(const std::shared_ptr<resource>& self) const
{
    ff::dict dict;

    for (auto& sprite : this->sprites)
    {
        std::ostringstream resource_name;
        resource_name << self->name() << "." << sprite.name();

        std::shared_ptr<ff::resource_object_base> sprite_resource = std::make_shared<ff::sprite_resource>(std::string(sprite.name()), self);
        dict.set(resource_name.str(), ff::value::create<ff::resource_object_base>(std::move(sprite_resource)));
    }

    return dict;
}

bool ff::sprite_list::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    // Find all unique textures
    std::vector<const ff::texture*> textures;
    for (auto& sprite : this->sprites)
    {
        const ff::texture* texture = sprite.texture().get();
        if (std::find(textures.cbegin(), textures.cend(), texture) == textures.cend())
        {
            textures.push_back(texture);
        }
    }

    // Save textures to a vector of resource dicts
    {
        std::vector<ff::value_ptr> textures_vector;
        for (const ff::texture* texture : textures)
        {
            ff::dict dict;
            bool allow_compress = false;
            if (!ff::resource_object_base::save_to_cache_typed(*texture, dict, allow_compress))
            {
                assert(false);
                return false;
            }

            textures_vector.push_back(ff::value::create<ff::dict>(std::move(dict)));
        }

        dict.set<std::vector<ff::value_ptr>>("textures", std::move(textures_vector));
    }

    dict.set<size_t>("size", this->size());
    {
        auto sprite_bytes = std::make_shared<std::vector<uint8_t>>();
        {
            ff::data_writer writer(sprite_bytes);
            for (auto& sprite : this->sprites)
            {
                size_t texture_index = std::find(textures.cbegin(), textures.cend(), sprite.texture().get()) - textures.cbegin();
                const ff::dxgi::sprite_data& sprite_data = sprite.sprite_data();

                ff::save(writer, texture_index);
                ff::save(writer, sprite.name());
                ff::save(writer, sprite_data.texture_uv());
                ff::save(writer, sprite_data.world());
                ff::save(writer, sprite_data.type());
            }
        }

        std::shared_ptr<ff::data_base> value = std::make_shared<ff::data_vector>(std::move(sprite_bytes));
        dict.set<ff::data_base>("sprites", value, ff::saved_data_type::none);
    }

    return true;
}

std::shared_ptr<ff::resource_object_base> ff::internal::sprite_list_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    bool optimize = dict.get<bool>("optimize", true);
    size_t mip_count = dict.get<size_t>("mips", 1);
    DXGI_FORMAT format = ff::dxgi::parse_format(dict.get<std::string>("format", std::string("rgba32")));
    if (format == DXGI_FORMAT_UNKNOWN)
    {
        assert(false);
        format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    if (mip_count > 1 && !ff::dxgi::color_format(format))
    {
        context.add_error("MipMaps are only supported for color textures");
        return {};
    }

    if (optimize && !ff::dxgi::color_format(format) && !ff::dxgi::palette_format(format))
    {
        context.add_error("Can only optimize full color or palette textures");
        return {};
    }

    ff::dict sprites_dict = dict.get<ff::dict>("sprites");
    std::unordered_map<std::wstring, std::shared_ptr<ff::texture>> texture_views;
    std::vector<ff::sprite> sprites;

    for (auto& sprite_iter : sprites_dict)
    {
        ff::dict sprite_dict = sprite_iter.second->get<ff::dict>();
        ff::rect_float pos_rect = sprite_dict.get<ff::rect_float>("pos");
        ff::point_float pos;
        ff::point_float size;

        if (pos_rect != ff::rect_float{})
        {
            pos = pos_rect.top_left();
            size = pos_rect.size();
        }
        else
        {
            pos = sprite_dict.get<ff::point_float>("pos");
            size = sprite_dict.get<ff::point_float>("size");
        }

        ff::point_float offset = sprite_dict.get<ff::point_float>("offset", ff::point_float(size.x, 0));
        std::filesystem::path full_file = sprite_dict.get<std::string>("file");
        ff::point_float handle = sprite_dict.get<ff::point_float>("handle");
        ff::point_float scale = sprite_dict.get<ff::point_float>("scale", ff::point_float(1, 1));
        size_t repeat = sprite_dict.get<size_t>("repeat", 1);

        std::shared_ptr<ff::texture> texture_view;
        auto iter = texture_views.find(full_file.native());
        if (iter != texture_views.cend())
        {
            texture_view = iter->second;
        }
        else
        {
            std::shared_ptr<ff::texture> texture = std::make_shared<ff::texture>(full_file,
                (optimize && ff::dxgi::color_format(format)) ? DXGI_FORMAT_R8G8B8A8_UNORM : format,
                optimize ? 1 : mip_count);

            if (!*texture)
            {
                std::ostringstream str;
                str << "Failed to load texture file: " << full_file;
                context.add_error(str.str());
                return nullptr;
            }

            texture_view = texture;
            texture_views.try_emplace(std::wstring(full_file.native()), texture_view);
        }

        if (size == ff::point_float{} && handle == ff::point_float{})
        {
            size = texture_view->dxgi_texture()->size().cast<float>();
        }

        if (size.x < 1 || size.y < 1)
        {
            return nullptr;
        }

        for (size_t i = 0; i < repeat; i++)
        {
            std::ostringstream sprite_name;
            sprite_name << sprite_iter.first;

            if (repeat > 1)
            {
                sprite_name << "[" << i << "]";
            }

            ff::point_float top_left(pos.x + offset.x * i, pos.y + offset.y * i);
            ff::rect_float rect(top_left, top_left + size);
            sprites.emplace_back(sprite_name.str(), texture_view, rect, handle, scale, ff::dxgi::sprite_type::unknown);
        }
    }

    if (optimize)
    {
        std::vector<ff::sprite> new_sprites = ff::internal::optimize_sprites(sprites, format, mip_count);
        if (new_sprites.size() != sprites.size())
        {
            debug_fail_ret_val(nullptr);
        }

        std::swap(sprites, new_sprites);
    }

    return std::make_shared<ff::sprite_list>(std::move(sprites));
}

std::shared_ptr<ff::resource_object_base> ff::internal::sprite_list_factory::load_from_cache(const ff::dict& dict) const
{
    std::vector<std::shared_ptr<ff::texture>> texture_views;
    {
        std::vector<ff::value_ptr> texture_values = dict.get<std::vector<ff::value_ptr>>("textures");
        texture_views.reserve(texture_values.size());

        for (auto& value : texture_values)
        {
            std::shared_ptr<ff::texture> texture = std::dynamic_pointer_cast<ff::texture>(value->get<ff::resource_object_base>());
            assert_ret_val(texture, nullptr);
            texture_views.push_back(texture);
        }
    }

    size_t size = dict.get<size_t>("size");
    std::vector<ff::sprite> sprites;
    sprites.reserve(size);
    {
        std::shared_ptr<ff::data_base> sprites_data = dict.get<ff::data_base>("sprites");
        if (!sprites_data)
        {
            assert(false);
            return {};
        }

        ff::data_reader reader(sprites_data);
        for (size_t i = 0; i < size; i++)
        {
            size_t texture_index;
            std::string name;
            ff::rect_float texture_uv;
            ff::rect_float world;
            ff::dxgi::sprite_type type;

            if (ff::load(reader, texture_index) &&
                ff::load(reader, name) &&
                ff::load(reader, texture_uv) &&
                ff::load(reader, world) &&
                ff::load(reader, type) &&
                texture_index < texture_views.size())
            {
                auto& view = texture_views[texture_index];
                sprites.emplace_back(std::move(name), view, ff::dxgi::sprite_data(view->dxgi_texture().get(), texture_uv, world, type));
            }
            else
            {
                assert(false);
                return nullptr;
            }
        }
    }

    return std::make_shared<ff::sprite_list>(std::move(sprites));
}
