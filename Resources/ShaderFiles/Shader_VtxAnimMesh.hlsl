#include "Shader_Defines.hlsli"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
float4x4 g_BoneMatrices[512];

float g_fAlpha = 1.f;

texture2D g_DiffuseTexture;
texture2D g_SpecularTexture;
texture2D g_DissolveTexture;
float g_fHasSpecularMap;
float g_fDissolveAmount = 0.f;
float g_fDissolveEdgeWidth = 0.08f;
float4 g_vDissolveEdgeColor = float4(1.f, 0.05f, 0.02f, 1.f);

struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD;
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
    uint4 vBlendIndex : BLENDINDEX;
    float4 vBlendWeight : BLENDWEIGHT;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
    float4 vProjPos : TEXCOORD2;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    float fWeightW = 1.f - (In.vBlendWeight.x + In.vBlendWeight.y + In.vBlendWeight.z);

    float4x4 BoneMatrix =
                g_BoneMatrices[In.vBlendIndex.x] * In.vBlendWeight.x +
                g_BoneMatrices[In.vBlendIndex.y] * In.vBlendWeight.y +
                g_BoneMatrices[In.vBlendIndex.z] * In.vBlendWeight.z +
                g_BoneMatrices[In.vBlendIndex.w] * fWeightW;

    float4 vPosition = mul(float4(In.vPosition, 1.f), BoneMatrix);
    float4 vNormal = mul(float4(In.vNormal, 0.f), BoneMatrix);

    float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(vPosition, matWVP);
    Out.vNormal = normalize(mul(vNormal, g_WorldMatrix));
    Out.vTexcoord = In.vTexcoord;
    Out.vProjPos = Out.vPosition;

    return Out;
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
    float4 vProjPos : TEXCOORD2;
};

struct PS_OUT
{
    float4 vDiffuse : SV_TARGET0;
    float4 vNormal : SV_TARGET1;
    float4 vDepth : SV_TARGET2;
};

struct PS_OUT_SHADOW
{
    float4 vLightDepth : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out;

    vector vMtrlDiffuse = g_DiffuseTexture.Sample(LinearSampler, In.vTexcoord);
    vMtrlDiffuse.a = 1.f;

    if (g_fDissolveAmount > 0.f)
    {
        float fNoise = g_DissolveTexture.Sample(LinearSampler, In.vTexcoord).r;
        float fEdge = smoothstep(g_fDissolveAmount, g_fDissolveAmount + g_fDissolveEdgeWidth, fNoise);

        if (fNoise < g_fDissolveAmount)
            discard;

        vMtrlDiffuse.rgb = lerp(g_vDissolveEdgeColor.rgb, vMtrlDiffuse.rgb, fEdge);
    }

    Out.vDiffuse = vMtrlDiffuse;
    float fSpecMask = g_SpecularTexture.Sample(LinearSampler, In.vTexcoord).r * g_fHasSpecularMap;
    Out.vNormal = vector(In.vNormal.xyz * 0.5f + 0.5f, fSpecMask);
    Out.vDepth = vector(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / 500.f, 0.f, 0.f);

    return Out;
}

PS_OUT_SHADOW PS_MAIN_SHADOW(PS_IN In)
{
    PS_OUT_SHADOW Out;
    
    Out.vLightDepth = vector((In.vProjPos.w / 2000.f), 0.f, 0.f, 0.f);
    
    return Out;
}

technique11 DefaultTechnique
{
    pass DefaultPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN();
    }

    pass BlendPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN();
    }

    pass ShadowPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_SHADOW();
    }
}
