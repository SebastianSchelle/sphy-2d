#include <bgfx_shader.sh>
#include <../common.sc>

uniform vec4 u_time;
uniform vec4 u_screenPos;

#define iterations 17
#define formuparam 0.53

#define volsteps 20
#define stepsize 0.15

#define zoom   0.0800
#define tile   0.850
#define speed  0.000000010 

#define brightness 0.0002
#define darkmatter 0.300
#define distfading 0.730
#define saturation 0.850

float fbm(vec2 st) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < 10; i++) {
        value += noise(st * frequency) * amplitude;
        frequency *= 2.0;  // double scale each octave
        amplitude *= 0.5;  // reduce influence
    }

    return value;
}

vec4 nebula(vec2 uv, float time) {
    // Slowly move UVs for animation
    vec2 uv1 = uv + vec2(0.05, 0.02) * time;
    vec2 uv2 = uv - vec2(0.02, 0.04) * time * 0.01;

    float n1 = fbm(uv1 * 2.0);
    float n2 = fbm(uv2 * 4.0);

	vec2 warp = vec2(fbm(uv * 3.0), fbm(uv * 3.0 + 5.0));
    float density = pow(n1 * n2, 2.0); // Multiply for more structure
	float alpha = smoothstep(0.05, 0.5, density);

	// Color gradient

    float c1 = fbm(uv*0.5);
    float c2 = fbm(uv*0.5);
	float cdensity = pow(c1 * c2, 2.0); // Multiply for more structure

    vec3 colA = vec3(0.1, 0.2, 0.4); // deep blue
    vec3 colB = vec3(0.8, 0.3, 0.2); // orange
    vec3 color = mix(colA, colB, cdensity);
	color *= alpha;

    return vec4(color, alpha);
}

void main() {
    vec2 uv = (gl_FragCoord.xy / u_viewRect.zw) - vec2(0.5,0.5);
    vec2 pos = uv * u_screenPos.zw + u_screenPos.xy;
    uv.y *= u_viewRect.w/u_viewRect.z;
    float time = u_time.x;
	vec3 dir=vec3(uv*zoom, 1.);

	vec3 from=vec3(1.,.5,0.5);
	from += vec3(u_screenPos.x*speed, u_screenPos.y*speed, 0.5);

    //volumetric rendering
	float s=0.1,fade=1.;
	vec3 v=vec3(0.);
	for (int r=0; r<volsteps; r++) {
		vec3 p=from+s*dir*.5;
		p = abs(vec3(tile)-mod(p,vec3(tile*2.))); // tiling fold
		float pa,a=pa=0.;
		for (int i=0; i<iterations; i++) { 
			p=abs(p)/dot(p,p)-formuparam; // the magic formula
			a+=abs(length(p)-pa); // absolute sum of average change
			pa=length(p);
		}
		float dm=max(0.,darkmatter-a*a*.001); //dark matter
		a*=a*a; // add contrast
		if (r>6) fade*=1.-dm; // dark matter, don't render near
		//v+=vec3(dm,dm*.5,0.);
		v+=fade;
		v+=vec3(s,s*s,s*s*s*s)*a*brightness*fade; // coloring based on distance
		fade*=distfading; // distance fading
		s+=stepsize;
	}
	v=mix(vec3(length(v)),v,saturation); //color adjust

	vec4 n = nebula(pos*0.0001, time);
	v = mix(v*0.01, n.rgb, n.a);
	gl_FragColor = vec4(v,1.);	
}