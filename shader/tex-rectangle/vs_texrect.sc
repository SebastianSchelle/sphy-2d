$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0
$output v_color0

#include <bgfx_shader.sh>

void main()
{
    float cosR = cos(i_data2);
    float sinR = sin(i_data2);
    mat2 rot = mat2(cosR, -sinR, sinR, cosR);

    vec2 scaled = a_position * i_data1;
    vec2 transformed = rot * scaled;
    vec2 worldPos = transformed + i_data0;
    gl_Position = mul(u_modelViewProj, vec4(worldPos, 0.1, 1.0) );
    v_texcoord0 = vec2(0.325, 0.5) + vec2(0.65, 1.0) * a_position; // text center pos + size * rect(1,1)
    v_color0 = i_data3;
}
