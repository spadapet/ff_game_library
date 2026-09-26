struct color_pixel
{
    float4 pos : SV_POSITION0;
    float4 color : COLOR0;
};

struct sprite_pixel
{
    float4 pos : SV_POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    uint indexes : INDEXES;
};

cbuffer vertex_shader_constants_0 : register(b0)
{
    matrix projection_;
    float2 view_scale_;
};

cbuffer vertex_shader_constants_1 : register(b1)
{
    matrix model_[128];
};

cbuffer pixel_shader_constants_0 : register(b2)
{
    float4 texture_palette_sizes_[32];
};

Texture2D textures_[32] : register(t0);
Texture2D<uint> palette_textures_[32] : register(t32);
Texture2D palette_ : register(t64);
Texture2D<uint> palette_remap_ : register(t65);
SamplerState samplers_[2] : register(s0);
