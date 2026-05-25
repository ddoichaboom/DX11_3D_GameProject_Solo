#include "Shader_Defines.hlsli"

// HLSL 코드 내에서 사용할 전역 변수 선언
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
texture2D g_Texture;
float4 g_vUVOffsetScale = float4(0.f, 0.f, 1.f, 1.f);
float g_fAlpha = 1.f;
float g_fGaugeProgress = 1.f;
float g_fGaugeVertical = 0.f;   // 0=horizontal / 1=vertical(bottom->top)
float4 g_vSweepTint = float4(1.f, 1.f, 1.f, 1.f);
float4 g_vUVOffset = float4(0.f, 0.f, 0.f, 0.f);
float4 g_vColor = float4(1.f, 1.f, 1.f, 1.f);

// 셰이더의 입력 구조체는 C++ 측 정점 구조체 VTXTEX와 1:1 대응 해야 함.
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

// 정점 셰이더
VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;
    
    // 1단계 - 월드 변환 : 로컬 -> 월드 (오브젝트를 세계에 배치)
    float4 vPosition = mul(float4(In.vPosition, 1.f), g_WorldMatrix);
    
    // 2단계 - 뷰 변환 : 월드 -> 뷰 (카메라 기준으로 변환)
    vPosition = mul(vPosition, g_ViewMatrix);
    
    // 3단계 - 투영 변환 : 뷰 -> 투영 (원근감 적용, 클리핑 준비
    vPosition = mul(vPosition, g_ProjMatrix);
    
    // 클립 좌표
    Out.vPosition = vPosition;
    Out.vTexcoord = In.vTexcoord;
    
    return Out;
}

VS_OUT VS_SPRITEANIM(VS_IN In)
{
    VS_OUT Out;
    
    float4 vPosition = mul(float4(In.vPosition, 1.f), g_WorldMatrix);
    vPosition = mul(vPosition, g_ViewMatrix);
    vPosition = mul(vPosition, g_ProjMatrix);

    Out.vPosition = vPosition;
    Out.vTexcoord = In.vTexcoord * g_vUVOffsetScale.zw + g_vUVOffsetScale.xy;

    return Out;
}

// w 나누기 연산 : 2차원 투영 스페이스로의 변환
// 뷰 포트로의 변환 (윈도우 좌표로의 변환)
// 레스터라이즈 (정점 정보를 기반으로 해서 픽셀의 정보를 생성)

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

struct PS_OUT
{
    float4 vColor : SV_TARGET0;
};

// 픽셀 셰이더 - 픽셀의 최종적인 색을 결정해준다
// 따라서 반환 타입 구조체는 멤버로 float4  vColor를 갖는다.

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out;
    
    Out.vColor = g_Texture.Sample(LinearSampler, In.vTexcoord);
    
    // Alpha Test 
    if (Out.vColor.a < 0.1f)
        discard;
    
    Out.vColor.gb = Out.vColor.r; // 회색처럼 표시 
    
    return Out;
}

PS_OUT PS_UI(PS_IN In)
{
    PS_OUT Out; 
    Out.vColor = g_Texture.Sample(LinearSampler, In.vTexcoord);
    
    // 기본 1.f 이므로 게이지 아닌 일반 UI는 영향 X
    if (In.vTexcoord.x > g_fGaugeProgress)
        discard;
    
    if (Out.vColor.a < 0.05f)
        discard;
    
    Out.vColor.a *= g_fAlpha;
    return Out;
}

PS_OUT PS_SWEEP(PS_IN In)
{
    PS_OUT Out;
    
    // UV-Sweep 일 때 g_vUVOffset이 0 이상, Position-Sweep 일때는 (0,0)으로 둔다.
    float2 vUV = In.vTexcoord + g_vUVOffset.xy;
    
    // title/wrap 처리 (UV-Sweep 시 부드러운 루프)
    vUV = frac(vUV);
    
    float4 vTex = g_Texture.Sample(LinearSampler, vUV);
    
    // 텍스처 알파만 마스크로 사용, 색은 tint로 덮어쓴다.
    float fMask = vTex.a;
    
    if (fMask < 0.05f)
        discard;
    
    Out.vColor.rgb = g_vSweepTint.rgb;
    Out.vColor.a = fMask * g_vSweepTint.a * g_fAlpha;
    
    return Out;
}

PS_OUT PS_SWEEP_GLOW(PS_IN In)
{
    PS_OUT Out;

      // 텍스처는 정상 sample (UV shift 없음)
    float4 vTex = g_Texture.Sample(LinearSampler, In.vTexcoord);
    float fMaskA = vTex.a;

    if (fMaskA < 0.05f)
        discard;

      // Y-band sweep mask (band 중심이 g_vUVOffset.y 위치, 폭 0.3)
    float fBandCenter = g_vUVOffset.y;
    float fBandWidth = 0.3f;
    float fDist = abs(In.vTexcoord.y - fBandCenter);

      // band 밖은 discard
    if (fDist > fBandWidth)
        discard;

      // 중심에 가까울수록 강한 빛 (부드러운 fade)
    float fBandIntensity = 1.0f - smoothstep(0.0f, fBandWidth, fDist);

    Out.vColor.rgb = g_vSweepTint.rgb;
    Out.vColor.a = fMaskA * g_vSweepTint.a * g_fAlpha * fBandIntensity;

    return Out;
}

PS_OUT PS_BOX(PS_IN In)
{
    PS_OUT Out;
    float2 uv = In.vTexcoord;
    float bw = 0.12f;
    bool edge = (uv.x < bw || uv.x > 1.f - bw || uv.y < bw || uv.y > 1.f - bw);
    Out.vColor = edge ? float4(1.f, 1.f, 1.f, 0.5f) : float4(g_vColor.rgb, g_vColor.a);
    Out.vColor.a *= g_fAlpha;
    return Out;
}

PS_OUT PS_FILL(PS_IN In)
{
    PS_OUT Out;
    Out.vColor = g_vColor;
    Out.vColor.a *= g_fAlpha;
    return Out;
}

PS_OUT PS_GAUGE_V(PS_IN In)
{
    PS_OUT Out;
    Out.vColor = g_Texture.Sample(LinearSampler, In.vTexcoord);
    if (In.vTexcoord.y < 1.f - g_fGaugeProgress)
        discard;
    if (Out.vColor.a < 0.05f)
        discard;
    Out.vColor.a *= g_fAlpha;
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

    pass UIPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_NONE, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_UI();
    }

    pass SpriteAnimPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_NONE, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_SPRITEANIM();
        PixelShader = compile ps_5_0 PS_UI();
    }

    pass SweepPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_NONE, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_SWEEP();
    }

    pass SweepGlowPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_NONE, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_SWEEP_GLOW();
    }

    pass BoxPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_NONE, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_BOX();
    }

    pass FillPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_NONE, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_FILL();
    }

    pass GaugeVPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_NONE, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        PixelShader = compile ps_5_0 PS_GAUGE_V();
    }
}

