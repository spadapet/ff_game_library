#include "data.hlsli"

struct line_vertex
{
    float2 start : POSITION0;
    float2 end : POSITION1;
    float2 before_start : POSITION2;
    float2 after_end : POSITION3;
    float4 start_color : COLOR0;
    float4 end_color : COLOR1;
    float start_thickness : THICKNESS0;
    float end_thickness : THICKNESS1;
    float depth : DEPTH;
    uint matrix_index : INDEX;
    uint vertex_id : SV_VertexID;
};

color_pixel vs_line(line_vertex input)
{
    const uint vx = input.vertex_id & 1; // 0 for start, 1 for end
    const uint vy = input.vertex_id >> 1; // 0 for top, 1 for bottom

    color_pixel output;
    output.pos = float4(lerp(input.start, input.end, vx), input.depth, 1);
    output.color = lerp(input.start_color, input.end_color, vx);

    float thickness = lerp(input.start_thickness, input.end_thickness, vx);
    if (thickness)
    {
        float2 dir = normalize(input.end - input.start);
        float2 prev_dir = input.start - input.before_start;
        float2 next_dir = input.after_end - input.end;
        prev_dir = normalize(lerp(dir, prev_dir, any(prev_dir)));
        next_dir = normalize(lerp(dir, next_dir, any(next_dir)));

        float2 perp = float2(-dir.y, dir.x);
        float2 prev_perp = float2(-prev_dir.y, prev_dir.x);
        float2 next_perp = float2(-next_dir.y, next_dir.x);

        float2 miter_dir = normalize(perp + lerp(prev_perp, next_perp, vx));
        float miter_length = rcp(max(dot(miter_dir, perp), 0.1));
        float2 miter_offset = miter_dir * thickness * 0.5 * miter_length;

        output.pos.xy += miter_offset * lerp(1, -1, vy);
    }

    output.pos = mul(output.pos, mul(model_[input.matrix_index], projection_));

    return output;
}
