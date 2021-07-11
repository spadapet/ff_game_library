#pragma once

namespace ff::internal::ui
{
    class render_device : public Noesis::RenderDevice, public ff::internal::graphics_child_base
    {
    public:
        render_device(bool srgb);
        virtual ~render_device() override;

        // Noesis::RenderDevice
        virtual const Noesis::DeviceCaps& GetCaps() const override;
        virtual Noesis::Ptr<Noesis::RenderTarget> CreateRenderTarget(const char* label, uint32_t width, uint32_t height, uint32_t sample_count, bool needs_stencil) override;
        virtual Noesis::Ptr<Noesis::RenderTarget> CloneRenderTarget(const char* label, Noesis::RenderTarget* surface) override;
        virtual Noesis::Ptr<Noesis::Texture> CreateTexture(const char* label, uint32_t width, uint32_t height, uint32_t mip_count, Noesis::TextureFormat::Enum format, const void** data) override;
        virtual void UpdateTexture(Noesis::Texture* texture, uint32_t level, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data) override;
        virtual void BeginOffscreenRender() override;
        virtual void EndOffscreenRender() override;
        virtual void BeginOnscreenRender() override;
        virtual void EndOnscreenRender() override;
        virtual void SetRenderTarget(Noesis::RenderTarget* surface) override;
        virtual void ResolveRenderTarget(Noesis::RenderTarget* surface, const Noesis::Tile* tiles, uint32_t tile_count) override;
        virtual void* MapVertices(uint32_t bytes) override;
        virtual void UnmapVertices() override;
        virtual void* MapIndices(uint32_t bytes) override;
        virtual void UnmapIndices() override;
        virtual void DrawBatch(const Noesis::Batch& batch) override;

        // graphics_child_base
        virtual bool reset() override;

    private:
        enum class msaa_samples_t { x1, x2, x4, x8, x16, Count };
        enum class texture_slot_t { Pattern, Ramps, Image, Glyphs, Shadow, PaletteImage, Palette, Count };

        struct vertex_shader_t
        {
            ID3D11VertexShader* shader();

            std::string_view resource_name;
            Microsoft::WRL::ComPtr<ID3D11VertexShader> shader_;
        };

        struct vertex_shader_and_layout_t : public vertex_shader_t
        {
            ID3D11InputLayout* layout();

            Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_;
            uint32_t layout_index;
        };

        struct pixel_shader_t
        {
            ID3D11PixelShader* shader();

            std::string_view resource_name;
            Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_;
        };

        struct sampler_state_t
        {
            ID3D11SamplerState* state();

            Noesis::SamplerState params;
            Microsoft::WRL::ComPtr<ID3D11SamplerState> state_;
        };

        struct vertex_and_pixel_program_t
        {
            int8_t vertex_shader_index;
            int8_t pixel_shader_index;
        };

        static Microsoft::WRL::ComPtr<ID3D11InputLayout> create_layout(uint32_t format, std::string_view vertex_resource_name);
        void create_buffers();
        void create_state_objects();
        void create_shaders();
        void clear_textures();
        void set_shaders(const Noesis::Batch& batch);
        void set_buffers(const Noesis::Batch& batch);
        void set_render_state(const Noesis::Batch& batch);
        void set_textures(const Noesis::Batch& batch);

        Noesis::DeviceCaps caps;

        // Device
        std::array<ID3D11ShaderResourceView*, static_cast<size_t>(texture_slot_t::Count)> null_textures;
#ifdef _DEBUG
        std::shared_ptr<ff::texture> empty_texture_rgb;
        std::shared_ptr<ff::texture> empty_texture_palette;
#endif

        // Buffers
        std::shared_ptr<ff::buffer> buffer_vertices;
        std::shared_ptr<ff::buffer> buffer_indices;
        std::shared_ptr<ff::buffer> buffer_vertex_cb[2];
        std::shared_ptr<ff::buffer> buffer_pixel_cb[3];
        uint32_t vertex_cb_hash[2];
        uint32_t pixel_cb_hash[3];

        // Shaders
        vertex_and_pixel_program_t programs[Noesis::Shader::Count];
        vertex_shader_and_layout_t vertex_stages[21];
        pixel_shader_t pixel_shaders[52];
        pixel_shader_t resolve_ps[static_cast<size_t>(msaa_samples_t::Count) - 1];
        vertex_shader_t quad_vs;

        // State
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states[2];
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_state_scissor;
        Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states[6];
        Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state_no_color;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_states[5];
        sampler_state_t sampler_states[64];
    };
}
