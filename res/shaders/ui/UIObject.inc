#ifndef HYP_UI_OBJECT
#define HYP_UI_OBJECT

#define UOFS_NONE 0x0
#define UOFS_HOVER 0x1
#define UOFS_PRESSED 0x2
#define UOFS_TOGGLED 0x4
#define UOFS_FOCUSED 0x8

#define UIObjectFocusState uint

#define UOB_NONE 0x0
#define UOB_TOP 0x1
#define UOB_LEFT 0x2
#define UOB_BOTTOM 0x4
#define UOB_RIGHT 0x8

#define UIObjectBorderFlags uint

struct UIObjectProperties
{
    uvec2 size;
    vec4 clamped_aabb;
    float border_radius;
    uint border_flags;
    UIObjectFocusState focus_state;
};

struct UIEntityInstanceBatch
{
    EntityInstanceBatch batch;
    vec4 texcoords[MAX_ENTITIES_PER_INSTANCE_BATCH];
    vec4 offsets[MAX_ENTITIES_PER_INSTANCE_BATCH];
    vec4 sizes[MAX_ENTITIES_PER_INSTANCE_BATCH];
    uvec4 properties[MAX_ENTITIES_PER_INSTANCE_BATCH];
};

UIObjectProperties GetUIObjectProperties(uvec4 data)
{
    UIObjectProperties properties;
    properties.border_radius = float(data[2] & 0xFFu);
    properties.border_flags = uint(data[2] >> 8u) & 0xFu;
    properties.focus_state = uint(data[2] >> 16u) & 0xFFu;
    properties.size = uvec2(data[0], data[1]);

    return properties;
}

#endif