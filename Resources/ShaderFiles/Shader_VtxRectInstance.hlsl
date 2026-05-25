#include "Shader_Defines.hlsli"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
texture2D g_Texture;

float g_fAlpha = 1.f;

struct VS_IN
{
    float3 vPosition : POSITION;
    float2 vTexcoord : TEXCOORD0;

    float4 vRight : TEXCOORD1;
    float4 vUp : TEXCOORD2;
    float4 vLook : TEXCOORD3;
    float4 vTranslation : TEXCOORD4;
    float4 vTexInfo : TEXCOORD5;
    float4 vColor : TEXCOORD6;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
    float4 vColor : TEXCOORD1;
};

VS_OUT VS_ATLAS_INSTANCE(VS_IN In)
{
    VS_OUT Out;

    float4x4 InstanceMatrix = float4x4(In.vRight, In.vUp, In.vLook, In.vTranslation);

    float4 vPosition = mul(float4(In.vPosition, 1.f), InstanceMatrix);
    vPosition = mul(vPosition, g_WorldMatrix);
    vPosition = mul(vPosition, g_ViewMatrix);
    vPosition = mul(vPosition, g_ProjMatrix);

    Out.vPosition = vPosition;
    Out.vTexcoord = In.vTexcoord * In.vTexInfo.zw + In.vTexInfo.xy;
    Out.vColor = In.vColor;

    return Out;
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
    float4 vColor : TEXCOORD1;
};

struct PS_OUT
{
    float4 vColor : SV_TARGET0;
};

PS_OUT PS_ATLAS_INSTANCE(PS_IN In)
{
    PS_OUT Out;

    Out.vColor = g_Texture.Sample(LinearSampler, In.vTexcoord);

    if (Out.vColor.a < 0.05f)
        discard;

    Out.vColor *= In.vColor;
    Out.vColor.a *= g_fAlpha;

    return Out;
}

PS_OUT PS_ATLAS_INSTANCE_ADDITIVE(PS_IN In)
{
    PS_OUT Out;

    float4 vTexColor = g_Texture.Sample(LinearSampler, In.vTexcoord);
    float fMaxRGB = max(max(vTexColor.r, vTexColor.g), vTexColor.b);

    if (fMaxRGB <= 0.12f)
        discard;

    float fIntensity = In.vColor.a * g_fAlpha;
    Out.vColor = float4(vTexColor.rgb * In.vColor.rgb * fIntensity, 0.f);

    return Out;
}

technique11 DefaultTechnique
{
    pass Atlas_Instance
    {
        SetRasterizerState(RS_Cull_None);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_ATLAS_INSTANCE();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_ATLAS_INSTANCE();
    }

    pass Atlas_Instance_Additive
    {
        SetRasterizerState(RS_Cull_None);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Blend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_ATLAS_INSTANCE();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_ATLAS_INSTANCE_ADDITIVE();
    }
}
