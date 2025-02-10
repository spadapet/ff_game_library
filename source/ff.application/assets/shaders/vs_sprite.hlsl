#include "data.hlsli"

struct sprite_vertex
{
    float4 rect : RECT;
    float4 uvrect : TEXCOORD;
    float4 color : COLOR;
    float4 posrot : POSROT;
    uint indexes : INDEXES;
    uint vertex_id : SV_VertexID;
};

sprite_pixel vs_sprite(sprite_vertex input)
{
    const uint vx = input.vertex_id & 1; // 0 for left, 1 for right
    const uint vy = input.vertex_id >> 1; // 0 for top, 1 for bottom

    sprite_pixel output;
    output.pos = float4(lerp(input.rect.x, input.rect.z, vx), lerp(input.rect.y, input.rect.w, vy), input.posrot.z, 1);
    output.uv = float2(lerp(input.uvrect.x, input.uvrect.z, vx), lerp(input.uvrect.y, input.uvrect.w, vy));
    output.color = input.color;
    output.indexes = input.indexes;

    if (input.posrot.w)
    {
        float rotate_sin, rotate_cos;
        sincos(input.posrot.w, rotate_sin, rotate_cos);
        output.pos.xy = float2(output.pos.x * rotate_cos + output.pos.y * rotate_sin, output.pos.y * rotate_cos - output.pos.x * rotate_sin);
    }

    output.pos.xy += input.posrot.xy;
    output.pos = mul(output.pos, mul(model_[input.indexes >> 24], projection_));

    return output;
}
