#version 330 core

out vec4 frag_color;

uniform vec2 resolution;
uniform vec2 center;
uniform float zoom;
uniform int max_iter;

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution;
    float aspect = resolution.x / resolution.y;
    float cx = (uv.x - 0.5) * zoom + center.x;
    float cy = (uv.y - 0.5) * (zoom / aspect) + center.y;

    float x = cx;
    float y = cy;
    int i;
    for (i = 0; i < max_iter; ++i) {
        float x2 = x*x - y*y + cx;
        float y2 = 2.0*x*y + cy;
        x = x2; y = y2;
        if (x*x + y*y > 4.0) break;
    }

    if (i == max_iter) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        float t = float(i) / float(max_iter);
        float brightness = pow(t, 0.5);
        frag_color = vec4(brightness, brightness * 0.5, brightness * 0.1, 1.0);
    }
}