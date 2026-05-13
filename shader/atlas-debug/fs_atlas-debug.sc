$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2DARRAY(u_texArray, 0);

uniform vec4 u_texLayer;

void main()
{
	float layer = u_texLayer.x;
	float mip = u_texLayer.y;
	vec3 uvw = vec3(v_texcoord0, layer);
	vec4 c = texture2DArrayLod(u_texArray, uvw, mip);
	// Premultiplied BGRA8: show over dark gray for transparent texels.
	vec3 bg = vec3(0.12, 0.12, 0.14);
	vec3 rgb = c.rgb + (1.0 - c.a) * bg;
	gl_FragColor = vec4(rgb, 1.0);
}
