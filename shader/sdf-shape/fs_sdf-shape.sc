$input v_uv, v_color, v_shape, v_thicknessY

#include <bgfx_shader.sh>

void main()
{
    float shapeType = v_shape.x;
    float thicknessX = v_shape.y;
    float thicknessY = v_thicknessY;

    float alpha;

    if (shapeType == 1.0) // RECT
    {
        if (thicknessX < 0.01 && thicknessY < 0.01)
        {
            alpha = 1.0;
        }
        else
        {
            float distToVertical = 1.0 - abs(v_uv.y);
            float distToHorizontal = 1.0 - abs(v_uv.x);
            if(distToVertical < thicknessY || distToHorizontal < thicknessX)
            {
                alpha = 1.0;
            }
            else
            {
                alpha = 0.0;
            }
        }
    }
    else if (shapeType == 2.0) // ELLIPSE: inner border with uniform world-space thickness
    {
        float dist = length(v_uv);
        float distToBoundary = 1.0 - dist;
        float tx = max(thicknessX, 0.001);
        float ty = max(thicknessY, 0.001);
        float radialScale = length(vec2(v_uv.x / tx, v_uv.y / ty)) / max(dist, 0.001);
        float distToInnerEdge = distToBoundary * 2.0 * radialScale;
        if (dist < 1.0 && distToInnerEdge < 1.0)
        {
            alpha = 1.0;
        }
        else
        {
            alpha = 0.0;
        }
    }
    else
    {
        alpha = 1.0;
    }

    gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
