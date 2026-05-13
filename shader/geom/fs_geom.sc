$input v_color0
$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2DARRAY(u_texArray,  0);

uniform vec4 u_texLayer;
uniform vec4 u_atlasPos;

void main()
{
    vec2 uv =  u_atlasPos.xy + v_texcoord0.xy * u_atlasPos.zw;
    vec3 uvw = vec3(uv, u_texLayer.x);
    vec4 texColor = texture2DArray(u_texArray, uvw);
    // Premultiplied atlas × premultiplied Rml vertex colour: modulate straight RGB, then premultiply.
    const float eps = 1.0 / 1024.0;
    vec3 tLin = texColor.a > eps ? texColor.rgb / texColor.a : vec3(0.0);
    vec3 vLin = v_color0.a > eps ? v_color0.rgb / v_color0.a : vec3(0.0);
    float outA = texColor.a * v_color0.a;
    vec4 finalColor = vec4(tLin * vLin * outA, outA);
    if (finalColor.a < 0.001)
        discard;
    gl_FragColor = finalColor;
}
