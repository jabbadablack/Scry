$input a_position, a_normal, i_data0, i_data1, i_data2, i_data3
$output v_normal, v_color

#include <bgfx_shader.sh>

void main()
{
    // Construct instance transform from columns
    mat4 instanceTransform = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
    
    vec4 worldPos = mul(instanceTransform, vec4(a_position, 1.0));
    gl_Position   = mul(u_viewProj, worldPos);

    v_normal = mul(instanceTransform, vec4(a_normal, 0.0)).xyz;
    
    // Hello Triangle style: Use position and normal to generate a multicolor effect
    v_color = vec4(a_position * 0.5 + 0.5, 1.0);
}