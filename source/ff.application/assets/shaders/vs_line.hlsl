#include "data.hlsli"

struct line_vertex
{
};

color_pixel vs_line(line_vertex input)
{
    color_pixel output = (color_pixel) 0;
    return output;
}

/*
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
*/
