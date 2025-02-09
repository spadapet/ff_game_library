#include "data.hlsli"

struct circle_vertex
{
    float4 position : POSITION;
    float4 inside_color : COLOR0;
    float4 outside_color : COLOR1;
    float radius : RADIUS;
    float thickness : THICKNESS;
    uint matrix_index : INDEX;
    uint vertex_id : SV_VertexID;
};

color_pixel vs_circle_filled(circle_vertex input)
{
    color_pixel output = (color_pixel) 0;
    return output;
}

color_pixel vs_circle_outline(circle_vertex input)
{
    color_pixel output = (color_pixel) 0;
    return output;
}

/*
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
*/
