#include "data.hlsli"

struct circle_vertex
{
    float2 cos_sin : COSSIN; // from vertex, rest is instance
    float4 position_radius : POSITION; // x, y, z=depth, w=radius
    float4 inside_color : COLOR0;
    float4 outside_color : COLOR1;
    float thickness : THICKNESS;
    uint matrix_index : INDEX;
    uint vertex_id : SV_VertexID;
};

color_pixel vs_circle(circle_vertex input)
{
    const uint v_inner = (input.vertex_id >> 5) & 1; // 0 for outer, 1 for inner

    color_pixel output;
    output.color = lerp(input.outside_color, input.inside_color, v_inner);
    output.pos = float4(input.cos_sin * lerp(input.position_radius.w, input.position_radius.w - input.thickness, v_inner) + input.position_radius.xy, input.position_radius.z, 1);
    output.pos = mul(output.pos, mul(model_[input.matrix_index], projection_));

    return output;
}
