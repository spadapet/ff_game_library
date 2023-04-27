#include "pch.h"
#include "test_app.h"
#include "utility.h"

#include <test_app.g.cpp>

static const ff::rect_float world_rect(0, 0, 1920, 1080);

namespace
{
    class app_state : public ff::state
    {
    public:
        app_state()
            : viewport(::world_rect.size().cast<size_t>())
            , font("font")
            , palette_data("palette")
            , palette_sprites("palette_sprites")
            , color_sprite("player_sprites.player")
            , palette_cycle(this->palette_data.object())
        {}

        virtual std::shared_ptr<ff::state> advance_time() override
        {
            if (ff::input::keyboard().press_count(VK_DELETE))
            {
                this->pos_datas.clear();
                this->render_datas.clear();
            }

            for (pos_data& data : this->pos_datas)
            {
                data.pos += data.vel;

                if (data.pos.x < ::world_rect.left || data.pos.x >= ::world_rect.right)
                {
                    data.vel.x = -data.vel.x;
                }

                if (data.pos.y < ::world_rect.top || data.pos.y >= ::world_rect.bottom)
                {
                    data.vel.y = -data.vel.y;
                }
            }

            if (ff::input::keyboard().press_count(VK_SPACE))
            {
                this->add_entities();
            }

            this->palette_cycle.advance();

            return {};
        }

        virtual void render(ff::dxgi::command_context_base& context, ff::render_targets& targets)
        {
            ff::dxgi::target_base& target = targets.target(context);
            ff::rect_float view = this->viewport.view(target.size().physical_pixel_size()).cast<float>();
            ff::dxgi::draw_ptr draw = ff::dxgi_client().global_draw_device().begin_draw(context, target, &targets.depth(context), view, ::world_rect);
            assert_ret(draw);

            draw->push_palette(&this->palette_cycle);

            for (size_t i = 0; i < this->pos_datas.size(); i++)
            {
                const pos_data& pd = this->pos_datas[i];
                const render_data& rd = this->render_datas[i];
                draw->draw_sprite(*rd.sprite, ff::dxgi::transform(pd.pos, rd.scale, rd.rotate, rd.color));
            }

            std::string text = ff::string::concat("Entities:", this->pos_datas.size());
            this->font->draw_text(draw, text, ff::dxgi::transform(ff::point_float(20, 1040), ff::point_float(1, 1), 0, ff::dxgi::color_black()), ff::dxgi::color_white());
        }

    private:
        struct pos_data
        {
            ff::point_float pos;
            ff::point_float vel;
        };

        struct render_data
        {
            const ff::dxgi::sprite_data* sprite;
            DirectX::XMFLOAT4 color;
            ff::point_float scale;
            float rotate;
        };

        void add_entities()
        {
            auto color_sprite  = this->color_sprite.object();
            auto palette_sprites = this->palette_sprites.object();

            const size_t new_count = 5000;
            this->pos_datas.reserve(this->pos_datas.size() + new_count);
            this->render_datas.reserve(this->render_datas.size() + new_count);

            for (int i = 0; i < new_count; i++)
            {
                pos_data pd;
                render_data rd;

                size_t sprite = (size_t)std::rand() % (palette_sprites->size() * 4);
                bool use_palette = sprite < palette_sprites->size();

                pd.pos = ff::point_float((float)(std::rand() % 1920), (float)(std::rand() % 1080));
                pd.vel = ff::point_float((std::rand() % 21 - 10) / 2.0f, (std::rand() % 21 - 10) / 2.0f);
                rd.scale = use_palette ? ff::point_float(2, 2) : ff::point_float((std::rand() % 16) / 10.0f + 0.5f, (std::rand() % 16) / 10.0f + 0.5f);
                rd.color = use_palette ? ff::dxgi::color_white() : DirectX::XMFLOAT4((std::rand() % 65) / 64.0f, (std::rand() % 65) / 64.0f, (std::rand() % 65) / 64.0f, 1.0f);
                rd.rotate = ff::math::random_range(0.0f, 360.0f);
                rd.sprite = use_palette ? &palette_sprites->get(sprite)->sprite_data() : &color_sprite->sprite_data();

                this->pos_datas.push_back(pd);
                this->render_datas.push_back(rd);
            }
        }

        ff::viewport viewport;
        std::vector<pos_data> pos_datas;
        std::vector<render_data> render_datas;
        ff::auto_resource<ff::sprite_font> font;
        ff::auto_resource<ff::palette_data> palette_data;
        ff::auto_resource<ff::sprite_list> palette_sprites;
        ff::auto_resource<ff::sprite_base> color_sprite;
        ff::palette_cycle palette_cycle;
    };
}

winrt::test_uwp::implementation::test_app::test_app()
{}

void winrt::test_uwp::implementation::test_app::loaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args)
{
    ff::init_app_params app_params = ::test_uwp::get_init_app_params();
    app_params.use_swap_chain_panel = true;

    app_params.get_clear_color_func = [](DirectX::XMFLOAT4& color)
        {
            color = ff::dxgi::color_blue();
            return true;
        };

    app_params.create_initial_state_func = []()
        {
            return std::make_shared<::app_state>();
        };

    this->init_app = std::make_unique<ff::init_app>(app_params, ::test_uwp::get_init_ui_params());
}

void winrt::test_uwp::implementation::test_app::unloaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args)
{
    this->init_app.reset();
}
