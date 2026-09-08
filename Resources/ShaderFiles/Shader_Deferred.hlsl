  #include "Shader_Defines.hlsli"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
float4x4 g_ViewMatrixInverse, g_ProjMatrixInverse;
float4x4 g_ShadowLightViewMatrix, g_ShadowLightProjMatrix;

texture2D g_Texture;
texture2D g_NormalTexture;
texture2D g_DiffuseTexture;
texture2D g_ShadeTexture;
texture2D g_DepthTexture;
texture2D g_SpecularTexture;
texture2D g_LightDepthTexture;

vector g_vCamPosition;

vector g_vLightDir;
vector g_vLightPos;
float g_fLightRange;

vector g_vLightDiffuse;
vector g_vLightAmbient;
vector g_vLightSpecular;

vector g_vMtrlAmbient = 1.f;
vector g_vMtrlSpecular = 1.f;

struct VS_IN
{
    float3 vPosition : POSITION;
    float2 vTexcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    float4 vPosition = mul(float4(In.vPosition, 1.f), g_WorldMatrix);
    vPosition = mul(vPosition, g_ViewMatrix);
    vPosition = mul(vPosition, g_ProjMatrix);

    Out.vPosition = vPosition;
    Out.vTexcoord = In.vTexcoord;

    return Out;
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

struct PS_OUT_BACKBUFFER
{
    float4 vBackBuffer : SV_TARGET0;
};

PS_OUT_BACKBUFFER PS_MAIN_DEBUG(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    Out.vBackBuffer = g_Texture.Sample(LinearSampler, In.vTexcoord);

    return Out;
}

struct PS_OUT_LIGHT
{
    float4 vShade : SV_TARGET0;
    float4 vSpecular : SV_TARGET1;
};

PS_OUT_LIGHT PS_MAIN_DIRECTIONAL(PS_IN In)
{    
    PS_OUT_LIGHT Out = (PS_OUT_LIGHT) 0;
    
    vector vNormalDesc = g_NormalTexture.Sample(LinearSampler, In.vTexcoord);
    vector vDepthDesc = g_DepthTexture.Sample(LinearSampler, In.vTexcoord);
    float fViewZ = vDepthDesc.y * 500.f;

    float4 vNormal = float4(vNormalDesc.xyz * 2.f - 1.f, 0.f);

    Out.vShade = g_vLightDiffuse *
                (
                        saturate(dot(normalize(g_vLightDir) * -1.f, normalize(vNormal))) +
                        (g_vLightAmbient * g_vMtrlAmbient)
                );

    vector vReflect = reflect(normalize(g_vLightDir), normalize(vNormal));

    vector vWorldPos;
    vWorldPos.x = In.vTexcoord.x * 2.f - 1.f;
    vWorldPos.y = In.vTexcoord.y * -2.f + 1.f;
    vWorldPos.z = vDepthDesc.x;
    vWorldPos.w = 1.f;

    vWorldPos *= fViewZ;
    vWorldPos = mul(vWorldPos, g_ProjMatrixInverse);
    vWorldPos = mul(vWorldPos, g_ViewMatrixInverse);

    vector vLook = vWorldPos - g_vCamPosition;

    Out.vSpecular = (g_vLightSpecular * g_vMtrlSpecular) *
                pow(saturate(dot(normalize(vReflect) * -1.f, normalize(vLook))), 50.f) *
                vNormalDesc.a;

    return Out;
}

PS_OUT_LIGHT PS_MAIN_POINT(PS_IN In)
{
    PS_OUT_LIGHT Out = (PS_OUT_LIGHT) 0;

    vector vNormalDesc = g_NormalTexture.Sample(LinearSampler, In.vTexcoord);
    vector vDepthDesc = g_DepthTexture.Sample(LinearSampler, In.vTexcoord);
    float fViewZ = vDepthDesc.y * 500.f;

    float4 vNormal = float4(vNormalDesc.xyz * 2.f - 1.f, 0.f);

    vector vWorldPos;
    vWorldPos.x = In.vTexcoord.x * 2.f - 1.f;
    vWorldPos.y = In.vTexcoord.y * -2.f + 1.f;
    vWorldPos.z = vDepthDesc.x;
    vWorldPos.w = 1.f;

    vWorldPos *= fViewZ;
    vWorldPos = mul(vWorldPos, g_ProjMatrixInverse);
    vWorldPos = mul(vWorldPos, g_ViewMatrixInverse);

    vector vLightDir = vWorldPos - g_vLightPos;
    float fAtt = saturate((g_fLightRange - length(vLightDir)) / g_fLightRange);

    Out.vShade =
                (
                        g_vLightDiffuse *
                        (
                                saturate(dot(normalize(vLightDir) * -1.f, normalize(vNormal))) +
                                (g_vLightAmbient * g_vMtrlAmbient)
                        )
                ) * fAtt;

    vector vReflect = reflect(normalize(vLightDir), normalize(vNormal));
    vector vLook = vWorldPos - g_vCamPosition;

    Out.vSpecular =
                (
                        (g_vLightSpecular * g_vMtrlSpecular) *
                        pow(saturate(dot(normalize(vReflect) * -1.f, normalize(vLook))), 50.f)
                ) * fAtt * vNormalDesc.a;

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    vector vDiffuse = g_DiffuseTexture.Sample(LinearSampler, In.vTexcoord);

    if (0.f == vDiffuse.a)
        discard;

    vector vShade = g_ShadeTexture.Sample(LinearSampler, In.vTexcoord);
    vector vSpecular = g_SpecularTexture.Sample(LinearSampler, In.vTexcoord);

    Out.vBackBuffer = vDiffuse * vShade + saturate(vSpecular);

    vector vDepthDesc = g_DepthTexture.Sample(LinearSampler, In.vTexcoord);
    float fViewZ = vDepthDesc.y * 500.f;

    vector vWorldPos;

    vWorldPos.x = In.vTexcoord.x * 2.f - 1.f;
    vWorldPos.y = In.vTexcoord.y * -2.f + 1.f;
    vWorldPos.z = vDepthDesc.x;
    vWorldPos.w = 1.f;

    vWorldPos *= fViewZ;
    vWorldPos = mul(vWorldPos, g_ProjMatrixInverse);
    vWorldPos = mul(vWorldPos, g_ViewMatrixInverse);

    vector vLightPos = mul(vWorldPos, g_ShadowLightViewMatrix);
    vLightPos = mul(vLightPos, g_ShadowLightProjMatrix);

    if (vLightPos.w > 0.f)
    {
        float2 vShadowTexcoord;

        vShadowTexcoord.x = (vLightPos.x / vLightPos.w) * 0.5f + 0.5f;
        vShadowTexcoord.y = (vLightPos.y / vLightPos.w) * -0.5f + 0.5f;

        if (vShadowTexcoord.x >= 0.f && vShadowTexcoord.x <= 1.f &&
              vShadowTexcoord.y >= 0.f && vShadowTexcoord.y <= 1.f)
        {
            vector vLightDepthDesc = g_LightDepthTexture.Sample(PointClampSampler, vShadowTexcoord);
            
            float fCurrentLightDepth = vLightPos.w;
            float fShadowObjectDepth = vLightDepthDesc.x * 2000.f;

            if (fCurrentLightDepth - 0.1f > fShadowObjectDepth)
                Out.vBackBuffer *= 0.5f;
        }
    }

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED_DIFFUSE(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    Out.vBackBuffer = g_DiffuseTexture.Sample(LinearSampler, In.vTexcoord);

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED_NORMAL(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    Out.vBackBuffer = g_NormalTexture.Sample(LinearSampler, In.vTexcoord);

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED_DEPTH(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    vector vDepth = g_DepthTexture.Sample(LinearSampler, In.vTexcoord);

    Out.vBackBuffer = float4(vDepth.xxx, 1.f);

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED_SHADE(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    Out.vBackBuffer = g_ShadeTexture.Sample(LinearSampler, In.vTexcoord);

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED_SPECULAR(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    Out.vBackBuffer = g_SpecularTexture.Sample(LinearSampler, In.vTexcoord);

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED_LIGHT_DEPTH(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    vector vLightDepth = g_LightDepthTexture.Sample(LinearSampler, In.vTexcoord);

    Out.vBackBuffer = float4(vLightDepth.xxx, 1.f);

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_FORCE_ALPHA(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    Out.vBackBuffer = float4(0.f, 0.f, 0.f, 1.f);
    
    return Out;

}

technique11 DefaultTechnique
{
    pass Debug
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_DEBUG();
    }

    pass Directional
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Blend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_DIRECTIONAL();
    }

    pass Point
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Blend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_POINT();
    }

    pass Combined
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_COMBINED();
    }

    pass Combined_Diffuse
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_COMBINED_DIFFUSE();
    }

    pass Combined_Normal
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_COMBINED_NORMAL();
    }

    pass Combined_Depth
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_COMBINED_DEPTH();
    }

    pass Combined_Shade
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_COMBINED_SHADE();
    }

    pass Combined_Specular
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_COMBINED_SPECULAR();
    }

    pass ForceAlpha
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_AlphaOnly, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_FORCE_ALPHA();
    }

    pass Combined_LightDepth
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Z_Disable, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN_COMBINED_LIGHT_DEPTH();
    }
}