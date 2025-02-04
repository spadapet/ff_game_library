float hash_3_to_1(float3 p)
{
    const float3 k = float3(0.3183099, 0.3678794, 0.7071068); // 1/pi, 1/e, 1/sqrt(2)
    p = frac(p * k + dot(p, p.yzx + 19.19));
    return frac((p.x + p.y) * p.z);
}

bool discard_for_dither(float3 pos, float a)
{
    return a != 1 && hash_3_to_1(float3(pos.xy, a)) >= a;
}
