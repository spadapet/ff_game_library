#include "pch.h"
#include "graphics/random_sprite.h"

static size_t advance_count = 0;

ff::random_sprite::random_sprite(std::vector<count_t>&& repeat_counts, std::vector<count_t>&& sprite_counts, std::vector<sprite_t>&& sprites)
    : repeat_counts(std::move(repeat_counts))
    , sprite_counts(std::move(sprite_counts))
    , sprites(std::move(sprites))
    , next_advance(0)
    , repeat_count_weight(0)
    , sprite_count_weight(0)
    , sprite_weight(0)
{
    for (auto& i : this->repeat_counts)
    {
        this->repeat_count_weight += i.weight;
    }

    for (auto& i : this->sprite_counts)
    {
        this->sprite_count_weight += i.weight;
    }

    for (auto& i : this->sprites)
    {
        this->sprite_weight += i.weight;
    }
}

void ff::random_sprite::advance_time()
{
    ::advance_count++;
}

void ff::random_sprite::draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform, float frame, const ff::dict* params)
{
    this->draw_animation(draw, transform);
}

void ff::random_sprite::draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform) const
{
    for (const auto& i : this->pick_sprites())
    {
        i.first->draw_frame(draw, transform, i.second);
    }
}

bool ff::random_sprite::resource_load_complete(bool from_source)
{
    for (auto& i : this->sprites)
    {
        i.anim = std::dynamic_pointer_cast<ff::animation_base>(i.source.object());
    }

    return true;
}

std::vector<std::shared_ptr<ff::resource>> ff::random_sprite::resource_get_dependencies() const
{
    std::vector<std::shared_ptr<ff::resource>> deps;
    deps.reserve(this->sprites.size());

    for (auto& i : this->sprites)
    {
        if (i.source.resource())
        {
            deps.push_back(i.source.resource());
        }
    }

    return deps;
}

const std::vector<std::pair<ff::animation_base*, float>>& ff::random_sprite::pick_sprites() const
{
    if (::advance_count >= this->next_advance)
    {
        this->picked.clear();

        int repeat_count = 1;
        if (this->repeat_count_weight)
        {
            repeat_count = ff::math::random_range(0, this->repeat_count_weight - 1);
            for (const auto& i : this->repeat_counts)
            {
                if (repeat_count < i.weight)
                {
                    repeat_count = i.count;
                    break;
                }
                else
                {
                    repeat_count -= i.weight;
                }
            }
        }

        int sprite_count = 1;
        if (this->sprite_count_weight)
        {
            sprite_count = ff::math::random_range(0, this->sprite_count_weight - 1);
            for (const auto& i : this->sprite_counts)
            {
                if (sprite_count < i.weight)
                {
                    sprite_count = i.count;
                    break;
                }
                else
                {
                    sprite_count -= i.weight;
                }
            }
        }

        for (; sprite_count; sprite_count--)
        {
            int sprite = ff::math::random_range(0, this->sprite_weight - 1);
            for (const auto& i : this->sprites)
            {
                if (sprite < i.weight)
                {
                    if (i.anim)
                    {
                        float frame = ff::math::random_range(0.0f, i.anim->frame_length());
                        this->picked.push_back(std::make_pair(i.anim.get(), frame));
                    }

                    break;
                }
                else
                {
                    sprite -= i.weight;
                }
            }
        }

        assert(repeat_count > 0);
        this->next_advance = ::advance_count + static_cast<size_t>(repeat_count);
    }

    return this->picked;
}

bool ff::random_sprite::save_to_cache(ff::dict& dict) const
{
    std::vector<int> repeat_counts;
    std::vector<int> sprite_counts;
    std::vector<int> sprite_weights;
    ff::value_vector sprites;

    repeat_counts.reserve(this->repeat_counts.size() * 2);
    sprite_counts.reserve(this->sprite_counts.size() * 2);
    sprite_weights.reserve(this->sprites.size());
    sprites.reserve(this->sprites.size());

    for (auto& i : this->repeat_counts)
    {
        repeat_counts.push_back(i.count);
        repeat_counts.push_back(i.weight);
    }

    for (auto& i : this->sprite_counts)
    {
        sprite_counts.push_back(i.count);
        sprite_counts.push_back(i.weight);
    }

    for (auto& i : this->sprites)
    {
        ff::value_ptr value = ff::value::create<ff::resource>(i.source.resource());
        sprites.push_back(std::move(value));
        sprite_weights.push_back(i.weight);
    }

    dict.set<std::vector<int>>("repeat_counts", std::move(repeat_counts));
    dict.set<std::vector<int>>("sprite_counts", std::move(sprite_counts));
    dict.set<std::vector<int>>("sprite_weights", std::move(sprite_weights));
    dict.set<ff::value_vector>("sprites", std::move(sprites));

    return true;
}

static std::vector<ff::random_sprite::count_t> load_counts_from_source(const ff::value_vector& values)
{
    std::vector<ff::random_sprite::count_t> counts;

    for (ff::value_ptr value : values)
    {
        ff::dict dict2 = value->get<ff::dict>();

        ff::random_sprite::count_t count;
        count.count = dict2.get<int>("count", 1);
        count.weight = dict2.get<int>("weight", 1);

        assert(count.count >= 0 && count.weight > 0);
        counts.push_back(std::move(count));
    }

    return counts;
}

std::shared_ptr<ff::resource_object_base> ff::internal::random_sprite_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    const ff::value_vector& repeat_counts_values = dict.get<ff::value_vector>("repeat_counts");
    const ff::value_vector& sprite_counts_values = dict.get<ff::value_vector>("sprite_counts");
    const ff::value_vector& sprites_values = dict.get<ff::value_vector>("sprites");

    std::vector<ff::random_sprite::count_t> repeat_counts = ::load_counts_from_source(repeat_counts_values);
    std::vector<ff::random_sprite::count_t> sprite_counts = ::load_counts_from_source(sprite_counts_values);

    std::vector<ff::random_sprite::sprite_t> sprites;
    for (ff::value_ptr value : sprites_values)
    {
        ff::dict dict2 = value->get<ff::dict>();

        ff::random_sprite::sprite_t sprite;
        sprite.source = dict2.get<ff::resource>("sprite");
        sprite.weight = dict2.get<int>("weight", 1);
        sprites.push_back(std::move(sprite));
    }

    return std::make_shared<random_sprite>(std::move(repeat_counts), std::move(sprite_counts), std::move(sprites));
}

std::shared_ptr<ff::resource_object_base> ff::internal::random_sprite_factory::load_from_cache(const ff::dict& dict) const
{
    std::vector<int> repeat_counts = dict.get<std::vector<int>>("repeat_counts");
    std::vector<int> sprite_counts = dict.get<std::vector<int>>("sprite_counts");
    std::vector<int> sprite_weights = dict.get<std::vector<int>>("sprite_weights");
    ff::value_vector sprites = dict.get<ff::value_vector>("sprites");

    std::vector<ff::random_sprite::count_t> repeat_counts_final;
    std::vector<ff::random_sprite::count_t> sprite_counts_final;
    std::vector<ff::random_sprite::sprite_t> sprites_final;

    for (size_t i = 0; i < repeat_counts.size(); i += 2)
    {
        ff::random_sprite::count_t count;
        count.count = repeat_counts[i];
        count.weight = repeat_counts[i + 1];
        repeat_counts_final.push_back(std::move(count));
    }

    for (size_t i = 0; i < sprite_counts.size(); i += 2)
    {
        ff::random_sprite::count_t count;
        count.count = sprite_counts[i];
        count.weight = sprite_counts[i + 1];
        sprite_counts_final.push_back(std::move(count));
    }

    for (size_t i = 0; i < sprite_weights.size() && i < sprites.size(); i++)
    {
        ff::random_sprite::sprite_t sprite;
        sprite.weight = sprite_weights[i];

        std::shared_ptr<ff::resource> sprite_resource = sprites[i]->get<ff::resource>();
        if (sprite_resource)
        {
            sprite.source = ff::auto_resource<ff::resource_object_base>(sprite_resource);
        }

        sprites_final.push_back(std::move(sprite));
    }

    return std::make_shared<random_sprite>(std::move(repeat_counts_final), std::move(sprite_counts_final), std::move(sprites_final));
}
