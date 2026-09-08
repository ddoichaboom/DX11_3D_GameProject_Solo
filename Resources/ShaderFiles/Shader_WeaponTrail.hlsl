#include "Shader_Defines.hlsli"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
texture2D g_Texture;

struct VS_IN
{
    float3 vPosition : POSITION;
    float2 vTexcoord : TEXCOORD0;
    float4 vColor : COLOR0;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
    float4 vColor : COLOR0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    float4 vPosition = mul(float4(In.vPosition, 1.f), g_WorldMatrix);
    vPosition = mul(vPosition, g_ViewMatrix);
    vPosition = mul(vPosition, g_ProjMatrix);

    Out.vPosition = vPosition;
    Out.vTexcoord = In.vTexcoord;
    Out.vColor = In.vColor;

    return Out;
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
    float4 vColor : COLOR0;
};

struct PS_OUT
{
    float4 vColor : SV_TARGET0;
};

PS_OUT PS_TRAIL_ADDITIVE(PS_IN In)
{
    PS_OUT Out;

    float4 vTexColor = g_Texture.Sample(ClampSampler, In.vTexcoord);
    float fMask = max(max(vTexColor.r, vTexColor.g), vTexColor.b) * vTexColor.a;

    if (fMask <= 0.03f || In.vColor.a <= 0.01f)
        discard;

    Out.vColor = float4(In.vColor.rgb * fMask * In.vColor.a, 0.f);
    return Out;
}

technique11 DefaultTechnique
{
    pass Trail_Additive
    {
        SetRasterizerState(RS_Cull_None);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Blend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_TRAIL_ADDITIVE();
    }
}
