#pragma once

namespace ff::internal::ui
{
    class render_device : public Noesis::RenderDevice, private ff::dxgi::device_child_base
    {
    public:
        render_device(bool srgb = false);
        virtual ~render_device() override;

        void render_begin(ff::dxgi::command_context_base& context, bool for_offscreen = true);
        void render_begin(ff::dxgi::command_context_base& context, ff::dxgi::target_base& target, ff::dxgi::depth_base& depth, const ff::rect_size& view_rect);
        void render_end();

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

    private:
        struct shader_t
        {
            std::unordered_map<uint32_t, Microsoft::WRL::ComPtr<ID3D12PipelineState>, ff::stable_hash<uint32_t>> pipeline_states;
            uint32_t stride;
        };

        // device_child_base
        virtual void before_reset() override;
        virtual bool reset() override;

        void init_samplers();
        void init_root_signature();
        void init_shaders();
        std::pair<ID3D12PipelineState*, size_t> pipeline_state_and_stride(size_t shader_index, uint8_t render_state);

        Noesis::DeviceCaps caps;
        ff::dx12::commands* commands;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
        DXGI_FORMAT target_format;

        shader_t shaders[Noesis::Shader::Count];
        ff::dx12::buffer_upload vertex_buffer;
        ff::dx12::buffer_upload index_buffer;
        ff::dx12::buffer constant_buffers[5]; // 2 vs, 3 ps

        ff::dx12::descriptor_range samplers_cpu; // 64
        ff::dx12::descriptor_range empty_views_cpu; // 3 (constant buffer, color texture, palette texture)
    };
}
