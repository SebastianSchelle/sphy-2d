$input a_position
$output v_worldPos

#include <bgfx_shader.sh>

void main()
{
    vec4 worldH = mul(u_invViewProj, vec4(a_position.xy, 0.0, 1.0));
    v_worldPos = worldH.xy / worldH.w;
    gl_Position = mul(u_viewProj, vec4(v_worldPos.xy, 0.0, 1.0));
}
