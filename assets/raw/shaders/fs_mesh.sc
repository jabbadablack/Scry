$input v_normal

#include <bgfx_shader.sh>

void main()
{
    vec3 lightDir = normalize(vec3(0.5, 1.0, -0.5));
    vec3 normal = normalize(v_normal);
    
    // Simple directional lighting
    float diff = max(dot(normal, lightDir), 0.0) * 0.85 + 0.15;
    
    // Output magenta with lighting
    gl_FragColor = vec4(diff, 0.0, diff, 1.0);
}