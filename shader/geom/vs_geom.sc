$input a_position
$input a_color0
$input a_texcoord0

$output v_color0
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_translation;
uniform mat4 u_myproj;
uniform mat4 u_transform;

void main()
{
    vec4 position = vec4(a_position + u_translation.xy, 0.0, 1.0);
    // Use z = 0.5 to ensure it's in front (near=0, far=1000)
    position = mul(u_transform, position);
    gl_Position = mul(u_myproj, position);
    v_color0 = a_color0;
    v_texcoord0 = a_texcoord0;
}
