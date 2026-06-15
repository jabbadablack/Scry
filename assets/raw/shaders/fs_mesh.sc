$input v_normal, v_color

#include <bgfx_shader.sh>

uniform vec4 u_baseColor;

void main()
{
    vec3 lightDir = normalize(vec3(0.2, 1.0, -0.4));
    vec3 normal = normalize(v_normal);
    
    // NDF directional lighting
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Ambient term
    float ambient = 0.2;
    
    // Final color: (Base Material Color * Lighting) + subtle vertex gradient
    vec3 finalColor = (u_baseColor.rgb * diff) + (v_color.rgb * 0.1) + (u_baseColor.rgb * ambient);
    
    gl_FragColor = vec4(finalColor, u_baseColor.a);
}