$input a_position, a_normal, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_normal, v_color

#include <bgfx_shader.sh>

void main()
{
    mat4 world    = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
    vec4 worldPos = mul(world, vec4(a_position, 1.0));
    gl_Position   = mul(u_viewProj, worldPos);

    v_normal = normalize(mul(world, vec4(a_normal, 0.0)).xyz);
    v_color  = i_data4;  // per-instance RGBA from 5th instance slot
}
