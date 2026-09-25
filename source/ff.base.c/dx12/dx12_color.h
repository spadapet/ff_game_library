#pragma once

typedef enum ff_color_type
{
    ff_color_type_rgba,
    ff_color_type_palette,
} ff_color_type;

// A palette color carries an index instead of RGB so that palette and RGBA sprites can travel
// through the same vertex color field. ff_color_to_shader packs either one into a float4.
typedef struct ff_color
{
    union
    {
        struct
        {
            float r;
            float g;
            float b;
            float a;
        } rgba;

        struct
        {
            int32_t index;
            float alpha;
        } palette;
    };

    ff_color_type type;
} ff_color;

typedef struct ff_color_shader
{
    float r;
    float g;
    float b;
    float a;
} ff_color_shader;

ff_color ff_color_rgba(float r, float g, float b, float a);
ff_color ff_color_palette(int32_t index, float alpha);

bool ff_color_equal(ff_color l, ff_color r);
float ff_color_alpha(ff_color color);

// index_remap may be NULL. It must have 256 entries when provided.
ff_color_shader ff_color_to_shader(ff_color color, const uint8_t* index_remap);

ff_color ff_color_none(void);
ff_color ff_color_none_palette(void);
ff_color ff_color_white(void);
ff_color ff_color_black(void);
ff_color ff_color_red(void);
ff_color ff_color_green(void);
ff_color ff_color_blue(void);
ff_color ff_color_yellow(void);
ff_color ff_color_cyan(void);
ff_color ff_color_magenta(void);
