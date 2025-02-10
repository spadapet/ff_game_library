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

//color_pixel vs_line(line_vertex input)
//{
//    const uint vx = input.vertex_id & 1; // 0 = start, 1 = end
//    const uint vy = input.vertex_id >> 1; // 0 = top, 1 = bottom

//    float2 pos = lerp(input.start, input.end, vx);
//    float thickness = lerp(input.start_thickness, input.end_thickness, vx);
    
//    color_pixel output;
//    output.color = lerp(input.start_color, input.end_color, vx);

//    if (thickness)
//    {
//        float2 dir = normalize(input.end - input.start);
//        float2 perp = float2(-dir.y, dir.x);

//        float2 prev_dir = normalize(input.start - input.before_start);
//        float2 next_dir = normalize(input.after_end - input.end);

//        bool has_prev = any(input.start - input.before_start);
//        bool has_next = any(input.after_end - input.end);

//        float2 bisector = normalize((has_prev && vx == 0) ? (dir + prev_dir) :
//                                (has_next && vx == 1) ? (dir + next_dir) :
//                                perp);

//        float miter_scale = rcp(saturate(dot(bisector, perp)));
//        miter_scale = min(miter_scale, 5.0); // Clamp to avoid excessive stretching

//        pos += bisector * (thickness * 0.5 * miter_scale) * (vy * -2.0 + 1.0);
//    }


//    matrix transform_matrix = input.matrix_index ? mul(model_[input.matrix_index], projection_) : projection_;
//    output.pos = mul(float4(pos, input.depth, 1), transform_matrix);
//    return output;
//}

//color_pixel vs_line(line_vertex input)
//{
//    const uint vx = input.vertex_id & 1; // 0 = start, 1 = end
//    const uint vy = input.vertex_id >> 1; // 0 = top, 1 = bottom

//    float2 pos = lerp(input.start, input.end, vx);
//    float thickness = lerp(input.start_thickness, input.end_thickness, vx);

//    color_pixel output;
//    output.color = lerp(input.start_color, input.end_color, vx);

//    if (thickness)
//    {
//        float2 dir = normalize(input.end - input.start);
//        float2 perp = float2(-dir.y, dir.x);

//        float2 prev_dir = normalize(input.start - input.before_start);
//        float2 next_dir = normalize(input.after_end - input.end);
//        float2 ref_dir = vx ? next_dir : prev_dir;
//        float2 joint_dir = normalize(dir + ref_dir);
//        float miter_scale = 1.0;

//        // Ensure outward direction
//        if (cross(float3(joint_dir, 0.0), float3(dir, 0.0)).z < 0.0)
//        {
//            joint_dir = -joint_dir;
//        }

//        float cos_theta = dot(joint_dir, perp);
//        miter_scale = rcp(max(abs(cos_theta), 0.1)); // Avoid division by zero

//        pos += joint_dir * (thickness * 0.5 * miter_scale) * (vy * -2.0 + 1.0);
//    }

//    matrix transform_matrix = input.matrix_index ? mul(model_[input.matrix_index], projection_) : projection_;
//    output.pos = mul(float4(pos, input.depth, 1), transform_matrix);
//    return output;
//}

color_pixel vs_line(line_vertex input)
{
    const uint vx = input.vertex_id & 1; // 0 for start, 1 for end
    const uint vy = input.vertex_id >> 1; // 0 for top, 1 for bottom
    matrix transform_matrix = input.matrix_index ? mul(model_[input.matrix_index], projection_) : projection_;

    color_pixel output;
    output.color = lerp(input.start_color, input.end_color, vx);

    float2 pos = lerp(input.start, input.end, vx);
    float thickness = lerp(input.start_thickness, input.end_thickness, vx);
    
    if (thickness == 0)
    {
        output.pos = float4(pos, input.depth, 1);
    }
    else
    {
        float2 next_pos = lerp(input.end, input.after_end, vx);
        float2 prev_pos = lerp(input.before_start, input.start, vx);
        
        // Compute direction vectors
        float2 dir = normalize(input.end - input.start);
        float2 prev_dir = normalize(input.start - input.before_start);
        float2 next_dir = normalize(input.after_end - input.end);

        // Compute perpendicular vectors
        float2 perp = float2(-dir.y, dir.x);
        float2 prev_perp = float2(-prev_dir.y, prev_dir.x);
        float2 next_perp = float2(-next_dir.y, next_dir.x);

        // Compute miter vector
        float2 miter_dir = normalize(perp + (vx ? next_perp : prev_perp));

        // Compute miter length (avoid divide by zero)
        float miter_length = 1.0 / max(dot(miter_dir, perp), 0.1);

        // Scale thickness with miter
        float2 offset = miter_dir * thickness * 0.5 * miter_length;

        // Adjust position based on vertex ID (top or bottom)
        pos += offset * (vy == 0 ? 1 : -1);

        output.pos = float4(pos, input.depth, 1);
    }

    output.pos = mul(output.pos, transform_matrix);
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
