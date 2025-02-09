#include "data.hlsli"

struct rectangle_vertex
{
    float4 rect : RECT;
    float4 color : COLOR;
    float depth : DEPTH;
    float thickness : THICKNESS;
    uint matrix_index : INDEX;
    uint vertex_id : SV_VertexID;
};

color_pixel vs_rectangle(rectangle_vertex input)
{
    const uint vx = input.vertex_id & 1; // 0 for left, 1 for right
    const uint vy = (input.vertex_id >> 1) & 1; // 0 for top, 1 for bottom
    const uint vt = (input.vertex_id >> 2) & 1; // 0 for outside, 1 for inside
    const float offset = lerp(0, input.thickness, vt);

    color_pixel output;
    output.color = input.color;

    matrix transform_matrix = input.matrix_index ? mul(model_[input.matrix_index], projection_) : projection_;
    output.pos = float4(lerp(input.rect.x + offset, input.rect.z - offset, vx), lerp(input.rect.y + offset, input.rect.w - offset, vy), input.depth, 1);
    output.pos = mul(output.pos, transform_matrix);

    return output;
}
