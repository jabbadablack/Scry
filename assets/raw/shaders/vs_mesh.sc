$output v_normal, v_color

#include <bgfx_shader.sh>
#include <bgfx_compute.sh>

// Vertex SSBO: 2 × vec4 per vertex
//   [0] = {px, py, pz, u}
//   [1] = {nx, ny, nz, v}
BUFFER_RO(b_vertices,  vec4, 0);

// Instance SSBO: 5 × vec4 per instance (sorted by mesh on CPU)
//   [0-3] = world matrix columns
//   [4]   = RGBA color
BUFFER_RO(b_instances, vec4, 1);

// x = base instance offset for this mesh in the sorted SSBO
uniform vec4 u_drawParams;

void main()
{
    uint vBase = uint(gl_VertexID) * 2u;
    vec4 v0 = b_vertices[vBase];        // {px, py, pz, nx}  — first  4 floats of ScryVertex
    vec4 v1 = b_vertices[vBase + 1u];   // {ny, nz, u,  v }  — second 4 floats of ScryVertex

    vec3 position = v0.xyz;
    vec3 normal   = vec3(v0.w, v1.x, v1.y);

    uint iBase = (uint(gl_InstanceID) + uint(u_drawParams.x)) * 5u;
    mat4 world = mtxFromCols(
        b_instances[iBase],
        b_instances[iBase + 1u],
        b_instances[iBase + 2u],
        b_instances[iBase + 3u]
    );
    v_color = b_instances[iBase + 4u];

    vec4 worldPos = mul(world, vec4(position, 1.0));
    gl_Position   = mul(u_viewProj, worldPos);
    v_normal = normalize(mul(world, vec4(normal, 0.0)).xyz);
}
