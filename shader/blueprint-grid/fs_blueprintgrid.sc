$input v_worldPos

#include <bgfx_shader.sh>

/// .x = cell size in world units (>= ~0.5)
/// .y = major line every N cells (>= 1)
uniform vec4 u_grid;

void main()
{
    vec2 p = v_worldPos;
    float cell = max(u_grid.x, 0.25);
    int majorN = int(max(u_grid.y, 1.0));

    vec2 fr = fract(p / cell);
    float fx = min(fr.x, 1.0 - fr.x);
    float fy = min(fr.y, 1.0 - fr.y);
    float fine = 1.0 - smoothstep(0.0, 0.018, min(fx, fy));

    int iix = int(floor(p.x / cell));
    int iiy = int(floor(p.y / cell));
    bool majX = majorN > 0 && (iix % majorN) == 0;
    bool majY = majorN > 0 && (iiy % majorN) == 0;
    float major = 0.0;
    if (majX)
    {
        major = max(major, 1.0 - smoothstep(0.0, 0.045, fx));
    }
    if (majY)
    {
        major = max(major, 1.0 - smoothstep(0.0, 0.045, fy));
    }

    float lineMask = clamp(fine * 0.07 + major * 0.11, 0.0, 0.22);

    vec3 bg = vec3(0.035, 0.065, 0.12);
    vec3 fineCol = vec3(0.22, 0.42, 0.55);
    vec3 majCol = vec3(0.30, 0.48, 0.62);
    vec3 lineCol = mix(fineCol, majCol, major);

    vec3 col = mix(bg, lineCol, lineMask);
    gl_FragColor = vec4(col, 1.0);
}
