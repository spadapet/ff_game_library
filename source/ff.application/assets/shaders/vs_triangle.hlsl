#include "data.hlsli"

struct triangle_vertex
{
    float2 pos0 : POSITION0;
    float2 pos1 : POSITION1;
    float2 pos2 : POSITION2;
    float4 color0 : COLOR0;
    float4 color1 : COLOR1;
    float4 color2 : COLOR2;
    float depth : DEPTH;
    uint matrix_index : INDEX;
    uint vertex_id : SV_VertexID;
};

color_pixel vs_triangle(triangle_vertex input)
{
    const uint vx = input.vertex_id & 1; // Position 0 or 1
    const uint vy = input.vertex_id >> 1; // or 2

    color_pixel output;
    output.color = lerp(lerp(input.color0, input.color1, vx), input.color2, vy);

    matrix transform_matrix = input.matrix_index ? mul(model_[input.matrix_index], projection_) : projection_;
    output.pos = float4(lerp(lerp(input.pos0, input.pos1, vx), input.pos2, vy), input.depth, 1);
    output.pos = mul(output.pos, transform_matrix);

    return output;
}
