$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0
$output v_color0

#include <bgfx_shader.sh>

uniform mat4 u_myproj;

void main()
{
	float cosR = cos(i_data2.x);
	float sinR = sin(i_data2.x);
	mat2 rot = mat2(cosR, -sinR, sinR, cosR);

	vec2 scaled = a_position * i_data0.zw;
	vec2 transformed = rot * scaled;
	vec2 worldPos = transformed + i_data0.xy;
	gl_Position = mul(u_myproj, vec4(worldPos, 0.0, 1.0));

	// Same UV mapping as fs_geom: atlas origin + unit quad (0..1) * span
	vec2 unitUv = a_position + 0.5;
	vec2 atlasUv = i_data1.xy + unitUv * i_data1.zw;
	// .xy = atlas UV, .z = texture array layer (TextureIdentifier::layerIdx)
	v_texcoord0 = vec3(atlasUv, i_data2.y);
	v_color0 = i_data3;
}
