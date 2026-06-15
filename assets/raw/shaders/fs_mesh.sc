$input v_normal, v_color

#include <bgfx_shader.sh>

void main()
{
    vec3 lightDir  = normalize(vec3(0.2, 1.0, -0.4));
    float diff     = max(dot(normalize(v_normal), lightDir), 0.0);
    float ambient  = 0.2;
    vec3 finalColor = v_color.rgb * (diff + ambient);
    gl_FragColor    = vec4(finalColor, v_color.a);
}
