static const float PI_F = 3.1415926535897932384626433832795f;
static const float PI2_F = PI_F * 2;
static const float Z_OFFSET = 0;

struct line_geometry
{
    float2 pos0 : POSITION0;
    float2 pos1 : POSITION1;
    float2 pos2 : POSITION2;
    float2 pos3 : POSITION3;
    float4 color0 : COLOR0;
    float4 color1 : COLOR1;
    float thick0 : THICK0;
    float thick1 : THICK1;
    float depth : DEPTH;
    uint world : MATRIX;
};

struct circle_geometry
{
    float3 pos : POSITION;
    float4 insideColor : COLOR0;
    float4 outsideColor : COLOR1;
    float radius : RADIUS;
    float thickness : THICK;
    uint world : MATRIX;
};

struct triangle_geometry
{
    float2 pos0 : POSITION0;
    float2 pos1 : POSITION1;
    float2 pos2 : POSITION2;
    float4 color0 : COLOR0;
    float4 color1 : COLOR1;
    float4 color2 : COLOR2;
    float depth : DEPTH;
    uint world : MATRIX;
};

struct sprite_geometry
{
    float4 rect : RECT;
    float4 uvrect : TEXCOORD;
    float4 color : COLOR;
    float2 scale : SCALE;
    float3 pos : POSITION;
    float rotate : ROTATE;
    uint tex : TEXINDEX;
    uint world : MATRIX;
};

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
    uint tex : TEXINDEX0;
};

cbuffer geometry_shader_constants_0 : register(b0)
{
    matrix projection_;
    float2 view_scale_;
};

cbuffer geometry_shader_constants_1 : register(b1)
{
    matrix model_[1024];
};

cbuffer pixel_shader_constants_0 : register(b0)
{
    float4 texture_palette_sizes_[32];
};

Texture2D textures_[32] : register(t0);
Texture2D<uint> textures_palette_[32] : register(t32);
Texture2D palette_ : register(t64);
Texture2D<uint> palette_remap_ : register(t65);
SamplerState sampler_ : register(s0);

line_geometry line_vs(line_geometry input)
{
    return input;
}

circle_geometry circle_vs(circle_geometry input)
{
    return input;
}

triangle_geometry triangle_vs(triangle_geometry input)
{
    return input;
}

sprite_geometry sprite_vs(sprite_geometry input)
{
    return input;
}

float hash_3_to_1(float3 p)
{
    p = frac(p * .1031);
    p += dot(p, p.zyx + 31.32);
    return frac((p.x + p.y) * p.z);
}

bool discard_for_dither(float3 pos, float a)
{
    return a != 1 && hash_3_to_1(float3(pos.xy, a)) >= a;
}

bool miters_on_same_side_of_line(float2 dir_line, float2 miter1, float2 miter2)
{
    float3 line_dir_3 = float3(dir_line, 0);
    float3 offset1_3 = float3(miter1, 0);
    float3 offset2_3 = float3(miter2, 0);

    float3 cross1 = cross(line_dir_3, offset1_3);
    float3 cross2 = cross(line_dir_3, offset2_3);

    return dot(cross1, cross2) > 0;
}

[maxvertexcount(4)]
void line_gs(point line_geometry input[1], inout TriangleStream<color_pixel> output)
{
    color_pixel vertex;

    float thickness_scale;
    float2 aspect;
    if (input[0].thick0 < 0)
    {
        thickness_scale = -view_scale_.y;
        aspect = float2(view_scale_.y / view_scale_.x, 1);
    }
    else
    {
        thickness_scale = 1;
        aspect = float2(1, 1);
    }

    float2 pos0 = input[0].pos0;
    float2 pos1 = input[0].pos1;
    float2 pos2 = input[0].pos2;
    float2 pos3 = input[0].pos3;

    float line_valid = any(pos1 != pos2);
    float prev_valid = any(pos0 != pos1);
    float next_valid = any(pos2 != pos3);

    pos0 *= aspect;
    pos1 *= aspect;
    pos2 *= aspect;
    pos3 *= aspect;

    float2 dir_line = pos2 - pos1;
    float2 dir_prev = pos1 - pos0;
    float2 dir_next = pos3 - pos2;

    dir_line = normalize(float2(dir_line.x * line_valid + (1 - line_valid), dir_line.y * line_valid));
    dir_prev = normalize(float2(dir_prev.x * prev_valid + (1 - prev_valid) * dir_line.x, dir_prev.y * prev_valid + (1 - prev_valid) * dir_line.y));
    dir_next = normalize(float2(dir_next.x * next_valid + (1 - next_valid) * dir_line.x, dir_next.y * next_valid + (1 - next_valid) * dir_line.y));

    float2 normal = normalize(float2(-dir_line.y, dir_line.x));

    float2 tangent1 = normalize(dir_prev + dir_line);
    float2 miter1 = float2(-tangent1.y, tangent1.x);
    float miter_dot1 = dot(miter1, normal);
    float miter_length1 = (input[0].thick0 * thickness_scale / 2) / (miter_dot1 + (miter_dot1 == 0));
    miter1 *= miter_length1;

    float2 tangent2 = normalize(dir_line + dir_next);
    float2 miter2 = float2(-tangent2.y, tangent2.x);
    float miter_dot2 = dot(miter2, normal);
    float miter_length2 = (input[0].thick1 * thickness_scale / 2) / (miter_dot2 + (miter_dot2 == 0));
    miter2 *= miter_length2;

    float swap_side = !miters_on_same_side_of_line(dir_line, miter1, miter2);
    miter2 *= -2 * swap_side + 1;

    float z = input[0].depth + Z_OFFSET;
    matrix transform_matrix = mul(model_[input[0].world], projection_);
    float4 p1_up = mul(float4((pos1 + miter1) / aspect, z, 1), transform_matrix);
    float4 p1_down = mul(float4((pos1 - miter1) / aspect, z, 1), transform_matrix);
    float4 p2_up = mul(float4((pos2 + miter2) / aspect, z, 1), transform_matrix);
    float4 p2_down = mul(float4((pos2 - miter2) / aspect, z, 1), transform_matrix);

    vertex.pos = p1_down;
    vertex.color = input[0].color0;
    output.Append(vertex);

    vertex.pos = p1_up;
    output.Append(vertex);

    vertex.pos = p2_down;
    vertex.color = input[0].color1;
    output.Append(vertex);

    vertex.pos = p2_up;
    output.Append(vertex);

    output.RestartStrip();
}

[maxvertexcount(128)]
void circle_gs(point circle_geometry input[1], inout TriangleStream<color_pixel> output)
{
    color_pixel vertex;

    float2 thickness;
    if (input[0].thickness < 0)
    {
        thickness = input[0].thickness * -view_scale_;
    }
    else
    {
        thickness = float2(input[0].thickness, input[0].thickness);
    }

    float z = input[0].pos.z + Z_OFFSET;
    float2 center = input[0].pos.xy;
    float2 radius = float2(input[0].radius, input[0].radius);
    matrix transform_matrix = mul(model_[input[0].world], projection_);

    float pixel_size = length(radius / view_scale_);
    pixel_size = max(64, pixel_size);
    pixel_size = min(440, pixel_size);

    uint points = ((uint)((pixel_size - 8) / 8) + 9) & ~(uint)1;
    for (uint i = 0; i <= points; i++)
    {
        float2 sin_cos_angle;
        sincos(PI2_F * i * (i != points) / points, sin_cos_angle.y, sin_cos_angle.x);

        float2 outside_point = center + sin_cos_angle * radius;
        float2 inside_point = outside_point - sin_cos_angle * thickness;

        vertex.pos = mul(float4(inside_point, z, 1), transform_matrix);
        vertex.color = input[0].insideColor;
        output.Append(vertex);

        vertex.pos = mul(float4(outside_point, z, 1), transform_matrix);
        vertex.color = input[0].outsideColor;
        output.Append(vertex);
    }

    output.RestartStrip();
}

[maxvertexcount(3)]
void triangle_gs(point triangle_geometry input[1], inout TriangleStream<color_pixel> output)
{
    color_pixel vertex;

    float z = input[0].depth + Z_OFFSET;
    float4 p0 = float4(input[0].pos0, z, 1);
    float4 p1 = float4(input[0].pos1, z, 1);
    float4 p2 = float4(input[0].pos2, z, 1);

    matrix transform_matrix = mul(model_[input[0].world], projection_);
    p0 = mul(p0, transform_matrix);
    p1 = mul(p1, transform_matrix);
    p2 = mul(p2, transform_matrix);

    vertex.pos = p0;
    vertex.color = input[0].color0;
    output.Append(vertex);

    vertex.pos = p1;
    vertex.color = input[0].color1;
    output.Append(vertex);

    vertex.pos = p2;
    vertex.color = input[0].color2;
    output.Append(vertex);

    output.RestartStrip();
}

[maxvertexcount(4)]
void sprite_gs(point sprite_geometry input[1], inout TriangleStream<sprite_pixel> output)
{
    sprite_pixel vertex;
    vertex.color = input[0].color;
    vertex.tex = input[0].tex;

    float rotate_sin, rotate_cos;
    sincos(input[0].rotate, rotate_sin, rotate_cos);

    float2x2 rotate_matrix =
    {
        rotate_cos, -rotate_sin,
        rotate_sin, rotate_cos,
    };

    float4 rect = input[0].rect * float4(input[0].scale, input[0].scale);
    float z = input[0].pos.z + Z_OFFSET;
    float4 tl = float4(mul(rect.xy, rotate_matrix) + input[0].pos.xy, z, 1);
    float4 tr = float4(mul(rect.zy, rotate_matrix) + input[0].pos.xy, z, 1);
    float4 br = float4(mul(rect.zw, rotate_matrix) + input[0].pos.xy, z, 1);
    float4 bl = float4(mul(rect.xw, rotate_matrix) + input[0].pos.xy, z, 1);

    matrix transform_matrix = mul(model_[input[0].world], projection_);
    tl = mul(tl, transform_matrix);
    tr = mul(tr, transform_matrix);
    br = mul(br, transform_matrix);
    bl = mul(bl, transform_matrix);

    vertex.pos = bl;
    vertex.uv = input[0].uvrect.xw;
    output.Append(vertex);

    vertex.pos = tl;
    vertex.uv = input[0].uvrect.xy;
    output.Append(vertex);

    vertex.pos = br;
    vertex.uv = input[0].uvrect.zw;
    output.Append(vertex);

    vertex.pos = tr;
    vertex.uv = input[0].uvrect.zy;
    output.Append(vertex);

    output.RestartStrip();
}

float4 color_ps(color_pixel input) : SV_TARGET
{
    if (input.color.a == 0)
    {
        discard;
    }

    return input.color;
}

// Input: RGBA (R*256=index), Output: Palette index
uint palette_out_color_ps(color_pixel input) : SV_TARGET
{
    uint index = (uint)(input.color.r * 256) * (uint)(input.color.a != 0);
    if (index == 0 || discard_for_dither(input.pos.xyz, input.color.a))
    {
        discard;
    }

    return index;
}

float4 SampleSpriteTexture(float2 tex, uint ntex)
{
    switch (ntex)
    {
    case 0: return textures_[0].Sample(sampler_, tex);
    case 1: return textures_[1].Sample(sampler_, tex);
    case 2: return textures_[2].Sample(sampler_, tex);
    case 3: return textures_[3].Sample(sampler_, tex);
    case 4: return textures_[4].Sample(sampler_, tex);
    case 5: return textures_[5].Sample(sampler_, tex);
    case 6: return textures_[6].Sample(sampler_, tex);
    case 7: return textures_[7].Sample(sampler_, tex);
    case 8: return textures_[8].Sample(sampler_, tex);
    case 9: return textures_[9].Sample(sampler_, tex);
    case 10: return textures_[10].Sample(sampler_, tex);
    case 11: return textures_[11].Sample(sampler_, tex);
    case 12: return textures_[12].Sample(sampler_, tex);
    case 13: return textures_[13].Sample(sampler_, tex);
    case 14: return textures_[14].Sample(sampler_, tex);
    case 15: return textures_[15].Sample(sampler_, tex);
    case 16: return textures_[16].Sample(sampler_, tex);
    case 17: return textures_[17].Sample(sampler_, tex);
    case 18: return textures_[18].Sample(sampler_, tex);
    case 19: return textures_[19].Sample(sampler_, tex);
    case 20: return textures_[20].Sample(sampler_, tex);
    case 21: return textures_[21].Sample(sampler_, tex);
    case 22: return textures_[22].Sample(sampler_, tex);
    case 23: return textures_[23].Sample(sampler_, tex);
    case 24: return textures_[24].Sample(sampler_, tex);
    case 25: return textures_[25].Sample(sampler_, tex);
    case 26: return textures_[26].Sample(sampler_, tex);
    case 27: return textures_[27].Sample(sampler_, tex);
    case 28: return textures_[28].Sample(sampler_, tex);
    case 29: return textures_[29].Sample(sampler_, tex);
    case 30: return textures_[30].Sample(sampler_, tex);
    case 31: return textures_[31].Sample(sampler_, tex);
    default: return (float4)0;
    }
}

uint SamplePaletteSpriteTexture(int3 tex, uint ntex)
{
    switch (ntex)
    {
    case 0: return textures_palette_[0].Load(tex);
    case 1: return textures_palette_[1].Load(tex);
    case 2: return textures_palette_[2].Load(tex);
    case 3: return textures_palette_[3].Load(tex);
    case 4: return textures_palette_[4].Load(tex);
    case 5: return textures_palette_[5].Load(tex);
    case 6: return textures_palette_[6].Load(tex);
    case 7: return textures_palette_[7].Load(tex);
    case 8: return textures_palette_[8].Load(tex);
    case 9: return textures_palette_[9].Load(tex);
    case 10: return textures_palette_[10].Load(tex);
    case 11: return textures_palette_[11].Load(tex);
    case 12: return textures_palette_[12].Load(tex);
    case 13: return textures_palette_[13].Load(tex);
    case 14: return textures_palette_[14].Load(tex);
    case 15: return textures_palette_[15].Load(tex);
    case 16: return textures_palette_[16].Load(tex);
    case 17: return textures_palette_[17].Load(tex);
    case 18: return textures_palette_[18].Load(tex);
    case 19: return textures_palette_[19].Load(tex);
    case 20: return textures_palette_[20].Load(tex);
    case 21: return textures_palette_[21].Load(tex);
    case 22: return textures_palette_[22].Load(tex);
    case 23: return textures_palette_[23].Load(tex);
    case 24: return textures_palette_[24].Load(tex);
    case 25: return textures_palette_[25].Load(tex);
    case 26: return textures_palette_[26].Load(tex);
    case 27: return textures_palette_[27].Load(tex);
    case 28: return textures_palette_[28].Load(tex);
    case 29: return textures_palette_[29].Load(tex);
    case 30: return textures_palette_[30].Load(tex);
    case 31: return textures_palette_[31].Load(tex);
    default: return 0;
    }
}

// Texture: RGBA, Output: RGBA
float4 sprite_ps(sprite_pixel input) : SV_TARGET
{
    float4 color = input.color * SampleSpriteTexture(input.uv, input.tex);

    if (color.a == 0)
    {
        discard;
    }

    return color;
}

// Texture: Palette Index, Output: RGBA (needs active palette to map index -> RGB)
float4 sprite_palette_ps(sprite_pixel input) : SV_TARGET
{
    uint texture_index = input.tex & 0xFF;
    uint palette_index = (input.tex & 0xFF00) >> 8;
    uint remap_index = (input.tex & 0xFF0000) >> 16;

    uint index = SamplePaletteSpriteTexture(int3(input.uv * texture_palette_sizes_[texture_index].xy, 0), texture_index);
    if (index == 0)
    {
        discard;
    }

    index = palette_remap_.Load(int3(index, remap_index, 0));
    float4 color = input.color * palette_.Load(int3(index, palette_index, 0));
    if (color.a == 0)
    {
        discard;
    }

    return color;
}

// Texture: Alpha only, Output: RGBA (111A)
float4 sprite_alpha_ps(sprite_pixel input) : SV_TARGET
{
    float4 color = input.color * float4(1, 1, 1, SampleSpriteTexture(input.uv, input.tex).a);

    if (color.a == 0)
    {
        discard;
    }

    return color;
}

// Texture: RGBA, Output: Palette index
uint palette_out_sprite_ps(sprite_pixel input) : SV_TARGET
{
    uint texture_index = input.tex & 0xFF;
    uint remap_index = (input.tex & 0xFF0000) >> 16;

    float4 color = SampleSpriteTexture(input.uv, texture_index);
    color.a *= input.color.a;
    uint index = ((uint)((input.color.r != 1) * input.color.r * 256) + (uint)((input.color.r == 1) * color.r * 256)) * (uint)(color.a != 0);
    index = palette_remap_.Load(int3(index, remap_index, 0));

    if (index == 0 || discard_for_dither(input.pos.xyz, color.a))
    {
        discard;
    }

    return index;
}

// Texture: Palette index, Output: Palette index
uint palette_out_sprite_palette_ps(sprite_pixel input) : SV_TARGET
{
    uint texture_index = input.tex & 0xFF;
    uint remap_index = (input.tex & 0xFF0000) >> 16;

    uint index = SamplePaletteSpriteTexture(int3(input.uv * texture_palette_sizes_[texture_index].xy, 0), texture_index);
    index = ((uint)((input.color.r != 1) * input.color.r * 256) + (uint)((input.color.r == 1) * index)) * (uint)(input.color.a != 0);
    index = palette_remap_.Load(int3(index, remap_index, 0));

    if (index == 0 || discard_for_dither(input.pos.xyz, input.color.a))
    {
        discard;
    }

    return index;
}
