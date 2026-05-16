$input v_texcoord0, v_atlasUv, v_tileParams, v_color0

#include <bgfx_shader.sh>

SAMPLER2DARRAY(u_texArray,  0);

// fract(1.0)==0 on seams. Odd internal seams (1,3,…) need t~=1; outer quad edges
// tileSt==0 → t~=0, tileSt==scale → t~=1. No global inset (atlas already inset).
vec2 tileLocalUv(vec2 tileSt, vec2 tileScale)
{
	vec2 n = floor(tileSt);
	vec2 f = tileSt - n;
	vec2 eps = vec2_splat(1.0 / 65536.0);
	vec2 onSeam = step(f, eps);
	vec2 oddTile = step(vec2_splat(0.5), mod(n, vec2(2.0)));
	vec2 atOuterMax = step(tileScale - eps, tileSt);
	vec2 atOuterMin = step(tileSt, eps);
	vec2 useHigh = onSeam * (oddTile + atOuterMax);
	vec2 t = mix(f, vec2_splat(1.0) - eps, useHigh);
	vec2 useLow = onSeam * atOuterMin * (vec2(1.0) - useHigh);
	return mix(t, eps, useLow);
}

void main()
{
	vec2 tileSt = v_texcoord0.xy;
	vec2 tileScale = v_tileParams.zw;
	bool tiling = (abs(tileScale.x - 1.0) > 0.0001) || (abs(tileScale.y - 1.0) > 0.0001);

	vec2 t = tiling ? tileLocalUv(tileSt, tileScale) : clamp(tileSt, 0.0, 1.0);

	vec3 atlasUv = vec3(v_atlasUv.xy + t * v_atlasUv.zw, v_texcoord0.z);
	vec4 texColor = tiling
		? texture2DArrayLod(u_texArray, atlasUv, 0.0)
		: texture2DArray(u_texArray, atlasUv);
	float outA = texColor.a * v_color0.a;
	vec4 finalColor = vec4(texColor.rgb * v_color0.rgb * v_color0.a, outA);
	if (outA < 0.001)
		discard;
	gl_FragColor = finalColor;
}
