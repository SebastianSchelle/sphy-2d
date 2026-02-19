$input a_position, a_texcoord0, a_color0, a_texcoord1, a_texcoord2
$output v_uv, v_color, v_shape, v_thicknessY

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 0.0, 1.0));
    v_uv = a_texcoord0;
    v_color = a_color0;
    v_shape = a_texcoord1;
    v_thicknessY = a_texcoord2;
}
