#ifdef SCRY_DEBUG

#include <engine/debug/debug_draw.h>
#include <engine/debug/debug_ui.h>
#include "graphics_internal.h"

#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

using namespace Diligent;

struct DebugVertex { float pos[3]; float col[4]; };

static constexpr uint32_t MAX_DEBUG_VERTS = 20000u;   // 10 000 lines × 2

static RefCntAutoPtr<IPipelineState>         g_pLinePSO;
static RefCntAutoPtr<IShaderResourceBinding> g_pLineSRB;
static RefCntAutoPtr<IBuffer>                g_pLineVB;
static RefCntAutoPtr<IBuffer>                g_pLineParamsCB;

static DebugVertex g_Verts[MAX_DEBUG_VERTS];
static uint32_t    g_VertCount = 0;
static float       g_ViewProj[16] = {};

static char* FindShader(const char* name, uint32_t* outLen) {
    const char* dirs[] = {
        "assets/raw/shaders/",
        "../assets/raw/shaders/",
        "../../assets/raw/shaders/"
    };
    char path[512];
    for (int i = 0; i < 3; ++i) {
        snprintf(path, sizeof(path), "%s%s", dirs[i], name);
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        char* buf = (char*)malloc((size_t)sz + 1);
        size_t n = fread(buf, 1, (size_t)sz, f);
        buf[n] = '\0';
        fclose(f);
        if (outLen) *outLen = (uint32_t)n;
        return buf;
    }
    return nullptr;
}

extern "C" {

void DebugDraw_Init(void* /*device*/, void* /*swap_chain*/) {
    IRenderDevice* dev = g_pDevice;
    ISwapChain*    sc  = g_pSwapChain;
    if (!dev || !sc) return;

    {
        BufferDesc bd;
        bd.Name           = "DebugLineVB";
        bd.Usage          = USAGE_DYNAMIC;
        bd.BindFlags      = BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = MAX_DEBUG_VERTS * (uint32_t)sizeof(DebugVertex);
        dev->CreateBuffer(bd, nullptr, &g_pLineVB);
    }
    {
        BufferDesc bd;
        bd.Name           = "DebugLineParams";
        bd.Usage          = USAGE_DYNAMIC;
        bd.BindFlags      = BIND_UNIFORM_BUFFER;
        bd.CPUAccessFlags = CPU_ACCESS_WRITE;
        bd.Size           = 64u;
        dev->CreateBuffer(bd, nullptr, &g_pLineParamsCB);
    }
    {
        uint32_t len = 0;
        char* src = FindShader("debug_line.hlsl", &len);
        if (!src) {
            fprintf(stderr, "[DebugDraw] debug_line.hlsl not found\n");
            return;
        }

        ShaderCreateInfo sci;
        sci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        sci.Source         = src;
        sci.SourceLength   = len;

        RefCntAutoPtr<IShader> pVS, pPS;
        { sci.Desc.ShaderType = SHADER_TYPE_VERTEX; sci.Desc.Name = "DebugLineVS"; sci.EntryPoint = "VSMain"; dev->CreateShader(sci, &pVS); }
        { sci.Desc.ShaderType = SHADER_TYPE_PIXEL;  sci.Desc.Name = "DebugLinePS"; sci.EntryPoint = "PSMain"; dev->CreateShader(sci, &pPS); }
        free(src);

        if (!pVS || !pPS) return;

        // float3 pos at offset 0, float4 color at offset 12 — auto-computed
        LayoutElement layout[] = {
            LayoutElement{0, 0, 3, VT_FLOAT32},
            LayoutElement{1, 0, 4, VT_FLOAT32},
        };
        ShaderResourceVariableDesc vars[] = {
            {SHADER_TYPE_VERTEX, "DrawParams", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        };

        GraphicsPipelineStateCreateInfo ci;
        ci.PSODesc.Name                        = "DebugLinePSO";
        ci.PSODesc.PipelineType                = PIPELINE_TYPE_GRAPHICS;
        ci.PSODesc.ResourceLayout.Variables    = vars;
        ci.PSODesc.ResourceLayout.NumVariables = 1;
        ci.GraphicsPipeline.NumRenderTargets   = 1;
        ci.GraphicsPipeline.RTVFormats[0]      = sc->GetDesc().ColorBufferFormat;
        ci.GraphicsPipeline.DSVFormat          = TEX_FORMAT_UNKNOWN;
        ci.GraphicsPipeline.PrimitiveTopology  = PRIMITIVE_TOPOLOGY_LINE_LIST;
        ci.GraphicsPipeline.DepthStencilDesc.DepthEnable   = False;
        ci.GraphicsPipeline.InputLayout.LayoutElements     = layout;
        ci.GraphicsPipeline.InputLayout.NumElements        = 2;
        ci.pVS = pVS;
        ci.pPS = pPS;
        dev->CreateGraphicsPipelineState(ci, &g_pLinePSO);

        if (g_pLinePSO) {
            // Create SRB immediately and bind the VP constant buffer
            g_pLinePSO->CreateShaderResourceBinding(&g_pLineSRB, true);
            g_pLineSRB->GetVariableByName(SHADER_TYPE_VERTEX, "DrawParams")->Set(g_pLineParamsCB);
        }
    }
}

void DebugDraw_Shutdown(void) {
    g_pLineSRB.Release();
    g_pLinePSO.Release();
    g_pLineVB.Release();
    g_pLineParamsCB.Release();
}

void DebugDraw_SetViewProj(const float vp[16]) {
    memcpy(g_ViewProj, vp, 64u);
}

void DebugDraw_AddLine(const float a[3], const float b[3], const float color[4]) {
    // Do not accumulate geometry when the overlay is hidden — the buffer would
    // grow across frames and overflow MAX_DEBUG_VERTS without ever being flushed.
    if (!DebugUI_IsVisible()) return;
    if (g_VertCount + 2 > MAX_DEBUG_VERTS) return;
    DebugVertex* v = g_Verts + g_VertCount;
    memcpy(v[0].pos, a,     12u); memcpy(v[0].col, color, 16u);
    memcpy(v[1].pos, b,     12u); memcpy(v[1].col, color, 16u);
    g_VertCount += 2;
}

void DebugDraw_AddAABB(const float mn[3], const float mx[3], const float color[4]) {
    float c[8][3] = {
        {mn[0], mn[1], mn[2]}, {mx[0], mn[1], mn[2]},
        {mx[0], mx[1], mn[2]}, {mn[0], mx[1], mn[2]},
        {mn[0], mn[1], mx[2]}, {mx[0], mn[1], mx[2]},
        {mx[0], mx[1], mx[2]}, {mn[0], mx[1], mx[2]},
    };
    DebugDraw_AddLine(c[0], c[1], color); DebugDraw_AddLine(c[1], c[2], color);
    DebugDraw_AddLine(c[2], c[3], color); DebugDraw_AddLine(c[3], c[0], color);
    DebugDraw_AddLine(c[4], c[5], color); DebugDraw_AddLine(c[5], c[6], color);
    DebugDraw_AddLine(c[6], c[7], color); DebugDraw_AddLine(c[7], c[4], color);
    DebugDraw_AddLine(c[0], c[4], color); DebugDraw_AddLine(c[1], c[5], color);
    DebugDraw_AddLine(c[2], c[6], color); DebugDraw_AddLine(c[3], c[7], color);
}

void DebugDraw_Render(void) {
    // Snapshot and reset unconditionally so the buffer never leaks across frames,
    // regardless of which branch causes an early return below.
    uint32_t count = g_VertCount;
    g_VertCount = 0;

    if (!DebugUI_IsVisible() || count == 0 || !g_pLinePSO || !g_pLineVB) return;

    IDeviceContext* ctx = g_pContext;
    ISwapChain*     sc  = g_pSwapChain;

    {
        PVoid p = nullptr;
        ctx->MapBuffer(g_pLineParamsCB, MAP_WRITE, MAP_FLAG_DISCARD, p);
        if (p) { memcpy(p, g_ViewProj, 64u); ctx->UnmapBuffer(g_pLineParamsCB, MAP_WRITE); }
    }
    {
        PVoid p = nullptr;
        ctx->MapBuffer(g_pLineVB, MAP_WRITE, MAP_FLAG_DISCARD, p);
        if (p) { memcpy(p, g_Verts, count * sizeof(DebugVertex)); ctx->UnmapBuffer(g_pLineVB, MAP_WRITE); }
    }

    ITextureView* pRTV = sc->GetCurrentBackBufferRTV();
    ctx->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const Uint64 offset = 0;
    IBuffer* vbs[] = { g_pLineVB };
    ctx->SetVertexBuffers(0, 1, vbs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                          SET_VERTEX_BUFFERS_FLAG_RESET);
    ctx->SetPipelineState(g_pLinePSO);
    ctx->CommitShaderResources(g_pLineSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs draw;
    draw.NumVertices = count;
    draw.Flags       = DRAW_FLAG_VERIFY_ALL;
    ctx->Draw(draw);
}

} // extern "C"

#endif // SCRY_DEBUG
