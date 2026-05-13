$input a_position
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_atlasDbgView;

void main()
{
	// Unit quad a_position in [-1,1]; scale+translate to letterboxed NDC rect.
	vec2 pos = a_position.xy * u_atlasDbgView.xy + u_atlasDbgView.zw;
	gl_Position = vec4(pos, 0.0, 1.0);
	// UV: full layer; Y down in atlas space (match prior fullscreen mapping).
	v_texcoord0 = vec2(a_position.x * 0.5 + 0.5, -a_position.y * 0.5 + 0.5);
}
