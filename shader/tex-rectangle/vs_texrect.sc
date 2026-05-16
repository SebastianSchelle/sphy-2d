$input a_position, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_texcoord0
$output v_atlasUv
$output v_tileParams
$output v_color0

#include <bgfx_shader.sh>

uniform mat4 u_myproj;

void main()
{
	float c = cos(i_data2.x);
	float s = sin(i_data2.x);

	vec2 scaled = a_position * i_data0.zw;
	vec2 transformed = vec2(
		scaled.x * c - scaled.y * s,
		scaled.x * s + scaled.y * c);
	vec2 worldPos = transformed + i_data0.xy;
	gl_Position = mul(u_myproj, vec4(worldPos, i_data2.z, 1.0));

	vec2 tileScale = i_data3.zw;
	vec2 tileOffset = i_data3.xy;
	vec2 unitUv = a_position + 0.5;
	vec2 tileSt = unitUv * tileScale + tileOffset;
	v_texcoord0 = vec3(tileSt, i_data2.y);
	v_atlasUv = i_data1;
	v_tileParams = i_data3;
	v_color0 = i_data4;
}
