$input a_position, a_normal, i_data0, i_data1, i_data2, i_data3
$output v_normal

#include <bgfx_shader.sh>

void main()
{
    // BGFX instancing passes the matrix as 4 vec4s.
    // Construct the instance transform matrix
    mat4 instanceTransform = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
    
    // Transform position: world space then view-projection
    vec4 worldPos = mul(instanceTransform, vec4(a_position, 1.0));
    gl_Position   = mul(u_viewProj, worldPos);

    // Transform normal (ignoring non-uniform scale for simplicity)
    v_normal = mul(instanceTransform, vec4(a_normal, 0.0)).xyz;
}