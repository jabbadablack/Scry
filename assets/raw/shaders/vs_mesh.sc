$output v_normal, v_color

#include <bgfx_shader.sh>
#include <bgfx_compute.sh>

BUFFER_RO(b_vertices,  vec4, 0);
BUFFER_RO(b_instances, vec4, 1);

uniform vec4 u_drawParams;

void main()
{
    uint vBase = uint(gl_VertexID) * 2u;
    vec4 v0 = b_vertices[vBase];       // {px, py, pz, nx}
    vec4 v1 = b_vertices[vBase + 1u];  // {ny, nz, u,  v}

    vec3 position = v0.xyz;
    vec3 normal   = vec3(v0.w, v1.x, v1.y);

    uint iBase = (uint(gl_InstanceID) + uint(u_drawParams.x)) * 4u;
    vec4 c0 = b_instances[iBase];
    vec4 c1 = b_instances[iBase + 1u];
    vec4 c2 = b_instances[iBase + 2u];
    vec4 c3 = b_instances[iBase + 3u];

    vec4 worldPos = c0 * position.x + c1 * position.y + c2 * position.z + c3;
    gl_Position   = mul(u_viewProj, worldPos);

    v_normal = normalize(c0.xyz * normal.x + c1.xyz * normal.y + c2.xyz * normal.z);
    v_color  = vec4(1.0, 1.0, 1.0, 1.0);
}
