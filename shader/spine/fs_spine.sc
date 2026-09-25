$input v_color0
$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2DARRAY(u_texArray,  0);

void main()
{
    vec2 uv =  v_texcoord0.xy;
    vec3 uvw = vec3(uv, v_texcoord0.z);
    vec4 texColor = texture2DArray(u_texArray, uvw);
    // Premultiplied atlas × premultiplied Rml vertex colour: modulate straight RGB, then premultiply.
    // const float eps = 1.0 / 1024.0;
    // vec3 tLin = texColor.a > eps ? texColor.rgb / texColor.a : vec3(0.0);
    // vec3 vLin = v_color0.a > eps ? v_color0.rgb / v_color0.a : vec3(0.0);
    // float outA = texColor.a * v_color0.a;
    // vec4 finalColor = vec4(tLin * vLin * outA, outA);
    vec4 finalColor = texColor * v_color0;
    if (finalColor.a < 0.001)
        discard;
    gl_FragColor = finalColor;
}
