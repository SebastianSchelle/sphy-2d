$input a_position
$input a_color0
$input a_texcoord0

$output v_color0
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_translation;

void main()
{
    vec2 position = a_position + u_translation.xy;
    gl_Position = vec4(position, 0.1, 1.00);
    v_color0 = a_color0;
    v_texcoord0 = a_texcoord0;
}
