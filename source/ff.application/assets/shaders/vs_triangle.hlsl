#include "data.hlsli"

struct triangle_filled_vertex
{
};

color_pixel vs_triangle_filled(triangle_filled_vertex input)
{
    color_pixel output = (color_pixel) 0;
    return output;
}

/*
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
*/
