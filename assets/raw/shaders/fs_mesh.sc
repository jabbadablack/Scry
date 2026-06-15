$input v_normal, v_color

#include <bgfx_shader.sh>

uniform vec4 u_baseColor;

void main()
{
    vec3 lightDir = normalize(vec3(0.5, 1.0, -0.5));
    vec3 normal = normalize(v_normal);
    
    float diff = max(dot(normal, lightDir), 0.0) * 0.85 + 0.15;
    
    // Mix vertex color with material base color
    gl_FragColor = v_color * u_baseColor * diff;
}