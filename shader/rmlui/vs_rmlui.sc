$input a_position
$input a_color0
$input a_texcoord0

$output v_color0
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_translation;
uniform mat4 u_myproj;

void main()
{
    vec2 position = a_position + u_translation.xy;
    // Use z = 0.5 to ensure it's in front (near=0, far=1000)
    gl_Position = mul(u_myproj, vec4(position, 0.5, 1.0));
    v_color0 = a_color0;
    v_texcoord0 = a_texcoord0;
}
