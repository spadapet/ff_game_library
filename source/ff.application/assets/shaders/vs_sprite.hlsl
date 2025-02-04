#include "data.hlsli"

struct sprite_vertex
{
    float4 rect : RECT;
    float4 uvrect : TEXCOORD;
    float4 color : COLOR;
    float depth : DEPTH;
    uint indexes : INDEXES;
    uint vertex_id : SV_VertexID;
};

struct rotated_sprite_vertex
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
    output.pos = float4(vx ? input.rect.z : input.rect.x, vy ? input.rect.w : input.rect.y, input.depth, 1);
    output.color = input.color;
    output.uv = float2(vx ? input.uvrect.z : input.uvrect.x, vy ? input.uvrect.w : input.uvrect.y);
    output.indexes = input.indexes;

    matrix transform_matrix = mul(model_[input.indexes >> 24], projection_);
    output.pos = mul(output.pos, transform_matrix);

    return output;
}

sprite_pixel vs_rotated_sprite(rotated_sprite_vertex input)
{
    const uint vx = input.vertex_id & 1; // 0 for left, 1 for right
    const uint vy = input.vertex_id >> 1; // 0 for top, 1 for bottom

    sprite_pixel output;
    output.color = input.color;
    output.uv = float2(vx ? input.uvrect.z : input.uvrect.x, vy ? input.uvrect.w : input.uvrect.y);
    output.indexes = input.indexes;
    
    float rotate_sin, rotate_cos;
    sincos(input.posrot.w, rotate_sin, rotate_cos);
    
    float2 pos_xy = float2(vx ? input.rect.z : input.rect.x, vy ? input.rect.w : input.rect.y);
    output.pos = float4(
        pos_xy.x * rotate_cos - pos_xy.y * rotate_sin + input.posrot.x,
        pos_xy.x * rotate_sin + pos_xy.y * rotate_cos + input.posrot.y,
        input.posrot.z, 1);

    matrix transform_matrix = mul(model_[input.indexes >> 24], projection_);
    output.pos = mul(output.pos, transform_matrix);

    return output;
}
