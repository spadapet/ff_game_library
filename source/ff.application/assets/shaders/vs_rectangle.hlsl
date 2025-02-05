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

color_pixel vs_rectangle_filled(rectangle_vertex input)
{
    color_pixel output = (color_pixel) 0;
    return output;
}

color_pixel vs_rectangle_outline(rectangle_vertex input)
{
    color_pixel output = (color_pixel) 0;
    return output;
}
