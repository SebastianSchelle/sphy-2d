$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0
$output v_color0

#include <bgfx_shader.sh>

uniform mat4 u_myproj;

void main()
{
	// Match rotateVec2 + vs_sdf-shape: +angle is CW (X right, Y down).
	float c = cos(i_data2.x);
	float s = sin(i_data2.x);

	vec2 scaled = a_position * i_data0.zw;
	vec2 transformed = vec2(
		scaled.x * c - scaled.y * s,
		scaled.x * s + scaled.y * c);
	vec2 worldPos = transformed + i_data0.xy;
	gl_Position = mul(u_myproj, vec4(worldPos, i_data2.z, 1.0));

	// Same UV mapping as fs_geom: atlas origin + unit quad (0..1) * span
	vec2 unitUv = a_position + 0.5;
	vec2 atlasUv = i_data1.xy + unitUv * i_data1.zw;
	// .xy = atlas UV, .z = texture array layer (TextureIdentifier::layerIdx)
	v_texcoord0 = vec3(atlasUv, i_data2.y);
	v_color0 = i_data3;
}
