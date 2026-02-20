$input a_position, a_texcoord0, a_color0, a_texcoord1, a_texcoord2, a_texcoord3
$output v_uv, v_color, v_shape, v_thicknessY

#include <bgfx_shader.sh>

void main()
{
    vec2 center = a_texcoord3.xy;
    float rot = a_texcoord3.z;
    float c = cos(rot), s = sin(rot);
    vec2 offset = a_position;
    vec2 worldPos = center + vec2(offset.x * c - offset.y * s, offset.x * s + offset.y * c);
    gl_Position = mul(u_modelViewProj, vec4(worldPos, 0.0, 1.0));
    v_uv = a_texcoord0;
    v_color = a_color0;
    v_shape = a_texcoord1;
    v_thicknessY = a_texcoord2;
}
