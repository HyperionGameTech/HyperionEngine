struct RayPayload
{
	vec4 color;
	vec3 normal;
	float distance;
	float roughness;
#if defined(PATHTRACER) || defined(LIGHTMAPPER) || defined(DDGI)
	vec4 throughput;
	vec4 emissive;
#endif
#ifdef LIGHTMAPPER
	vec3 barycentric_coords;
#endif
};

struct RayProbePayload
{
	vec3 color;
	vec3 normal;
	float distance;
	float rand;
	vec3 diffuse;
};

struct RayHit
{
	RayPayload payload;
	vec3 position;
};

struct PackedVertex
{
    float position_x;
    float position_y;
    float position_z;
    float normal_x;
    float normal_y;
    float normal_z;
    float texcoord_s;
    float texcoord_t;
};