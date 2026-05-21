$input v_worldPos

#include <bgfx_shader.sh>

/// .x = cell size in world units (meters)
/// .y = highlight strength on alternating cells (premultiplied alpha)
uniform vec4 u_grid;

void main()
{
	float cell = max(u_grid.x, 1.0);
	float strength = clamp(u_grid.y, 0.0, 1.0);

	ivec2 cellId = ivec2(floor(v_worldPos / cell));
	if (((cellId.x + cellId.y) & 1) == 0)
	{
		discard;
	}

	gl_FragColor = vec4(vec3_splat(strength), strength);
}
