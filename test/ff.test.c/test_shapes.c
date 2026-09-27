#include "pch.h"
#include "test_app.h"

#define SHAPES_WORLD_WIDTH 1920.0f
#define SHAPES_WORLD_HEIGHT 1080.0f
#define SHAPES_STAR_POINTS 11
#define SHAPES_PI 3.14159265f

static struct
{
    ff_dx12_draw_state draw_state;
    ff_dx12_draw_device draw_device;
} s_shapes;

static ff_dx12_draw_endpoint shape_point(float x, float y, ff_color color, float size)
{
    ff_dx12_draw_endpoint point;
    point.pos.x = x;
    point.pos.y = y;
    point.color = color;
    point.size = size;

    return point;
}

static bool shapes_init(ff_test_app* app)
{
    FF_CHECK_RET_VAL(ff_dx12_draw_state_init(&s_shapes.draw_state), false);

    if (!ff_dx12_draw_device_init(&s_shapes.draw_device, &s_shapes.draw_state))
    {
        ff_dx12_draw_state_destroy(&s_shapes.draw_state);
        return false;
    }

    return true;
}

static void shapes_destroy(ff_test_app* app)
{
    ff_dx12_draw_device_destroy(&s_shapes.draw_device);
    ff_dx12_draw_state_destroy(&s_shapes.draw_state);
}

static void draw_rectangles(ff_dx12_draw_device* device, float phase)
{
    const float wobble = 40.0f * sinf(phase);

    ff_dx12_draw_device_draw_rectangle(device,
        ff_rect_float_make(80.0f, 80.0f, 400.0f, 300.0f), ff_color_red(), 0.0f);

    ff_dx12_draw_device_draw_rectangle(device,
        ff_rect_float_make(440.0f, 80.0f, 760.0f, 300.0f), ff_color_green(), 8.0f + wobble * 0.1f);

    // Negative thickness puts the outline outside the rect, so this frame sits around the filled
    // one above rather than inside its own bounds.
    ff_dx12_draw_device_draw_rectangle(device,
        ff_rect_float_make(80.0f, 80.0f, 400.0f, 300.0f), ff_color_yellow(), -6.0f);
}

static void draw_translucent_stack(ff_dx12_draw_device* device, float phase)
{
    // Three overlapping translucent rectangles, drawn back to front. If the transparent pass ever
    // regressed to bucket order, the overlaps would composite in the wrong order and the colors
    // where they cross would visibly change.
    const float slide = 30.0f * sinf(phase);

    for (size_t i = 0; i < 3; i++)
    {
        const float offset = (float)i * 60.0f + slide;

        const ff_color color = (i == 0)
            ? ff_color_rgba(1.0f, 0.0f, 0.0f, 0.5f)
            : ((i == 1) ? ff_color_rgba(0.0f, 1.0f, 0.0f, 0.5f) : ff_color_rgba(0.0f, 0.0f, 1.0f, 0.5f));

        ff_dx12_draw_device_draw_rectangle(device,
            ff_rect_float_make(820.0f + offset, 80.0f + offset, 1120.0f + offset, 300.0f + offset),
            color, 0.0f);
    }
}

static void draw_circles(ff_dx12_draw_device* device, float phase)
{
    const float radius = 90.0f + 25.0f * sinf(phase);

    ff_dx12_draw_device_draw_circle(device,
        shape_point(1350.0f, 190.0f, ff_color_cyan(), radius), 0.0f, ff_color_cyan());

    // A gradient from the center color to the edge color, which only shows up when the two differ.
    ff_dx12_draw_device_draw_circle(device,
        shape_point(1620.0f, 190.0f, ff_color_white(), 100.0f), 0.0f, ff_color_magenta());

    ff_dx12_draw_device_draw_circle(device,
        shape_point(1350.0f, 460.0f, ff_color_yellow(), 100.0f), 12.0f, ff_color_yellow());
}

static void draw_triangles(ff_dx12_draw_device* device, float phase)
{
    const float lift = 40.0f * sinf(phase);

    const ff_dx12_draw_endpoint points[] =
    {
        shape_point(160.0f, 620.0f, ff_color_red(), 0.0f),
        shape_point(400.0f, 620.0f, ff_color_green(), 0.0f),
        shape_point(280.0f, 400.0f + lift, ff_color_blue(), 0.0f),

        shape_point(440.0f, 620.0f, ff_color_cyan(), 0.0f),
        shape_point(680.0f, 620.0f, ff_color_magenta(), 0.0f),
        shape_point(560.0f, 400.0f - lift, ff_color_yellow(), 0.0f),
    };

    ff_dx12_draw_device_draw_triangles(device, points, _countof(points));
}

// A closed star, which is the shape that actually exercises the miter wrap: every joint including
// the one at the seam has real neighbors on both sides. A bad wrap shows up as a notch there.
static void draw_star(ff_dx12_draw_device* device, float phase)
{
    ff_dx12_draw_endpoint points[SHAPES_STAR_POINTS];

    const float center_x = 960.0f;
    const float center_y = 700.0f;

    for (size_t i = 0; i < SHAPES_STAR_POINTS; i++)
    {
        // Stepping by 2/5 of a turn per point walks the pentagram; the eleventh point repeats the
        // first, which is what marks the polyline closed.
        const float angle = phase + (float)(i % (SHAPES_STAR_POINTS - 1)) * 4.0f * SHAPES_PI / 5.0f;

        points[i] = shape_point(
            center_x + 160.0f * cosf(angle),
            center_y + 160.0f * sinf(angle),
            ff_color_white(), 6.0f);
    }

    ff_dx12_draw_device_draw_lines(device, points, _countof(points));
}

static void draw_gradient_line(ff_dx12_draw_device* device, float phase)
{
    // Thickness and color both vary along this polyline, so it covers the per-endpoint fields that
    // a single-color line would leave untested.
    ff_dx12_draw_endpoint points[24];

    for (size_t i = 0; i < _countof(points); i++)
    {
        const float t = (float)i / (float)(_countof(points) - 1);

        points[i] = shape_point(
            200.0f + t * 1500.0f,
            900.0f + 70.0f * sinf(phase + t * 6.0f),
            ff_color_rgba(t, 1.0f - t, 0.5f, 1.0f),
            2.0f + 14.0f * t);
    }

    ff_dx12_draw_device_draw_lines(device, points, _countof(points));
}

static bool shapes_render(ff_test_app* app, ff_dx12_commands* commands)
{
    const size_t width = ff_test_app_width(app);
    const size_t height = ff_test_app_height(app);
    FF_CHECK_RET_VAL(width && height, false);

    ff_dx12_target_size target_size;
    target_size.pixel_size.x = width;
    target_size.pixel_size.y = height;
    target_size.rotation = ff_dx12_rotation_none;
    target_size.dpi_scale = 1.0;

    const ff_rect_float view_rect = ff_rect_float_make(0.0f, 0.0f, (float)width, (float)height);
    const ff_rect_float world_rect = ff_rect_float_make(0.0f, 0.0f, SHAPES_WORLD_WIDTH, SHAPES_WORLD_HEIGHT);

    FF_CHECK_RET_VAL(ff_dx12_draw_device_begin(&s_shapes.draw_device, commands,
        ff_dx12_target_window_resource(&app->target), ff_dx12_target_window_view(&app->target),
        target_size, ff_dx12_target_window_format(), NULL, view_rect, world_rect, false), false);

    const float phase = (float)app->frame / 40.0f;

    ff_dx12_commands_begin_event(commands, ff_dx12_gpu_event_draw_2d);

    draw_rectangles(&s_shapes.draw_device, phase);
    draw_translucent_stack(&s_shapes.draw_device, phase);
    draw_circles(&s_shapes.draw_device, phase);
    draw_triangles(&s_shapes.draw_device, phase);
    draw_star(&s_shapes.draw_device, phase);
    draw_gradient_line(&s_shapes.draw_device, phase);

    ff_dx12_draw_device_end(&s_shapes.draw_device);

    ff_dx12_commands_end_event(commands);

    return true;
}

const ff_test_mode ff_test_mode_shapes =
{
    .name = FF_SVL_INIT("shapes"),
    .description = FF_SVL_INIT("Rectangles, lines, circles and triangles, filled and outlined"),
    .init = shapes_init,
    .destroy = shapes_destroy,
    .render = shapes_render,
};
