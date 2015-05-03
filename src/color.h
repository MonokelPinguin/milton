// color.h
// (c) Copyright 2015 by Sergio Gonzalez.

typedef struct ColorPicker_s
{
    v2f a;  // Corresponds to value = 0      (black)
    v2f b;  // Corresponds to saturation = 0 (white)
    v2f c;  // Points to chosen hue.         (full color)

    v2i     center;  // In screen pixel coordinates.
    int32   bound_radius_px;
    Rect    bounds;
    float   wheel_radius;
    float   wheel_half_width;

    uint32* pixels;  // BLit this to render picker

    v3f     hsv;
} ColorPicker;

typedef enum
{
    ColorPickResult_nothing         = 0,
    ColorPickResult_change_color    = (1 << 1),
    ColorPickResult_redraw_picker   = (1 << 2),
} ColorPickResult;

typedef struct ColorManagement_s
{
    uint32 mask_a;
    uint32 mask_r;
    uint32 mask_g;
    uint32 mask_b;
    /* uint32 shift_a = find_least_significant_set_bit(mask_a).index; */
    /* uint32 shift_r = find_least_significant_set_bit(mask_r).index; */
    /* uint32 shift_g = find_least_significant_set_bit(mask_g).index; */
    /* uint32 shift_b = find_least_significant_set_bit(mask_b).index; */
    uint32 shift_a;
    uint32 shift_r;
    uint32 shift_g;
    uint32 shift_b;
} ColorManagement;

static void color_init(ColorManagement* cm)
{
    // TODO: This is platform dependant
    cm->mask_a = 0xff000000;
    cm->mask_r = 0x00ff0000;
    cm->mask_g = 0x0000ff00;
    cm->mask_b = 0x000000ff;
    cm->shift_a = find_least_significant_set_bit(cm->mask_a).index;
    cm->shift_r = find_least_significant_set_bit(cm->mask_r).index;
    cm->shift_g = find_least_significant_set_bit(cm->mask_g).index;
    cm->shift_b = find_least_significant_set_bit(cm->mask_b).index;
}


v3f hsv_to_rgb(v3f hsv)
{
    v3f rgb = { 0 };

    float h = hsv.x;
    float s = hsv.y;
    float v = hsv.z;
    float hh = h / 60.0f;
    int hi = (int)(floor(hh));
    float cr = v * s;
    float rem = fmodf(hh, 2.0f);
    float x = cr * (1.0f - absf(rem - 1.0f));
    float m = v - cr;

    switch (hi)
    {
    case 0:
        {
            rgb.r = cr;
            rgb.g = x;
            rgb.b = 0;
            break;
        }
    case 1:
        {
            rgb.r = x;
            rgb.g = cr;
            rgb.b = 0;
            break;
        }
    case 2:
        {
            rgb.r = 0;
            rgb.g = cr;
            rgb.b = x;
            break;
        }
    case 3:
        {
            rgb.r = 0;
            rgb.g = x;
            rgb.b = cr;
            break;
        }
    case 4:
        {
            rgb.r = x;
            rgb.g = 0;
            rgb.b = cr;
            break;
        }
    case 5:
        {
            rgb.r = cr;
            rgb.g = 0;
            rgb.b = x;
            //  don't break;
        }
    default:
        {
            break;
        }
    }
    rgb.r += m;
    rgb.g += m;
    rgb.b += m;
    assert (rgb.r >= 0.0f && rgb.r <= 1.0f);
    assert (rgb.g >= 0.0f && rgb.g <= 1.0f);
    assert (rgb.b >= 0.0f && rgb.b <= 1.0f);
    return rgb;
}

inline v3f sRGB_to_linear(v3f rgb)
{
    v3f result = rgb;
    float* d = result.d;
    for (int i = 0; i < 3; ++i)
    {
        if (*d <= 0.04045f)
        {
            *d /= 12.92f;
        }
        else
        {
            *d = powf((*d + 0.055f) / 1.055f, 2.4f);
        }
        ++d;
    }
    return result;
}

static bool32 picker_hits_wheel(ColorPicker* picker, v2f point, float* out_angle)
{
    v2f center = v2i_to_v2f(picker->center);
    v2f arrow = sub_v2f (point, center);
    float dist = magnitude(arrow);
    if (
            (dist <= picker->wheel_radius + picker->wheel_half_width ) &&
            (dist >= picker->wheel_radius - picker->wheel_half_width )
       )
    {
        if (out_angle)
        {
            v2f baseline = { 1, 0 };
            float dotp = (dot(arrow, baseline)) / (magnitude(arrow));
            float angle = acosf(dotp);
            if (point.y > center.y)
            {
                angle = (2 * kPi) - angle;
            }
            *out_angle = angle;
        }
        return true;
    }
    return false;
}

static bool32 is_inside_picker(ColorPicker* picker, v2i point)
{
    // Check if we hit the wheel

    return picker_hits_wheel(picker, v2i_to_v2f(point), NULL);
}

static Rect picker_get_draw_rect(ColorPicker* picker)
{
    Rect draw_rect;
    {
        draw_rect.left = picker->center.x - picker->bound_radius_px;
        draw_rect.right = picker->center.x + picker->bound_radius_px;
        draw_rect.bottom = picker->center.y + picker->bound_radius_px;
        draw_rect.top = picker->center.y - picker->bound_radius_px;
    }
    assert (draw_rect.left >= 0);
    assert (draw_rect.top >= 0);

    return draw_rect;
}

inline uint32 color_v4f_to_u32(ColorManagement cm, v4f c)
{
    uint32 result =
        ((uint8)(c.r * 255.0f) << cm.shift_r) |
        ((uint8)(c.g * 255.0f) << cm.shift_g) |
        ((uint8)(c.b * 255.0f) << cm.shift_b) |
        ((uint8)(c.a * 255.0f) << cm.shift_a);
    return result;
}

inline v4f color_u32_to_v4f(ColorManagement cm, uint32 color)
{
    v4f result =
    {
        (float)(0xff & (color >> cm.shift_r)) / 255,
        (float)(0xff & (color >> cm.shift_g)) / 255,
        (float)(0xff & (color >> cm.shift_b)) / 255,
        (float)(0xff & (color >> cm.shift_a)) / 255,
    };

    return result;
}

static ColorPickResult picker_update(ColorPicker* picker, v2i point)
{
    v2f fpoint = v2i_to_v2f(point);
    float angle = 0;
    if (picker_hits_wheel(picker, fpoint, &angle))
    {
        picker->hsv.h = radians_to_degrees(angle);
        return ColorPickResult_change_color;
    }
    return ColorPickResult_nothing;
}