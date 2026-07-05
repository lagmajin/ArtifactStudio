import os

base = r'X:\dev\ArtifactStudio\ArtifactCore\include\Graphics\Shader\HLSL\Blend'
os.makedirs(base, exist_ok=True)

BLEND_HEADER = '''Texture2D<float4> SrcTex  : register(t0);
Texture2D<float4> DstTex  : register(t1);
RWTexture2D<float4> ResultTex : register(u0);
'''

BLEND_FOOTER = '''[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));
    float3 blended = BlendFunc(dst.rgb, src.rgb);
    float outAlpha = src.a + dst.a * (1.0 - src.a);
    float3 outColor = (src.a * blended + dst.rgb * dst.a * (1.0 - src.a)) / max(outAlpha, 1e-5);
    ResultTex[DTid.xy] = float4(outColor, outAlpha);
}
'''

shaders = {
    'CS_BlendMultiply.hlsl': (
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) { return base * blend; }\n' +
        BLEND_FOOTER
    ),
    'CS_BlendLinearBurn.hlsl': (
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) { return saturate(base + blend - 1.0); }\n' +
        BLEND_FOOTER
    ),
    'CS_BlendDivide.hlsl': (
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) { return saturate(base / max(blend, 1e-5)); }\n' +
        BLEND_FOOTER
    ),
    'CS_BlendPinLight.hlsl': (
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) {\n'
        '    float3 r;\n'
        '    r.r = (blend.r < 0.5) ? min(base.r, 2.0*blend.r) : max(base.r, 2.0*(blend.r-0.5));\n'
        '    r.g = (blend.g < 0.5) ? min(base.g, 2.0*blend.g) : max(base.g, 2.0*(blend.g-0.5));\n'
        '    r.b = (blend.b < 0.5) ? min(base.b, 2.0*blend.b) : max(base.b, 2.0*(blend.b-0.5));\n'
        '    return saturate(r);\n'
        '}\n' +
        BLEND_FOOTER
    ),
    'CS_BlendVividLight.hlsl': (
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) {\n'
        '    float3 r;\n'
        '    r.r = (blend.r < 0.5) ? 1.0-(1.0-base.r)/max(2.0*blend.r,1e-5) : base.r/max(2.0*(1.0-blend.r),1e-5);\n'
        '    r.g = (blend.g < 0.5) ? 1.0-(1.0-base.g)/max(2.0*blend.g,1e-5) : base.g/max(2.0*(1.0-blend.g),1e-5);\n'
        '    r.b = (blend.b < 0.5) ? 1.0-(1.0-base.b)/max(2.0*blend.b,1e-5) : base.b/max(2.0*(1.0-blend.b),1e-5);\n'
        '    return saturate(r);\n'
        '}\n' +
        BLEND_FOOTER
    ),
    'CS_BlendLinearLight.hlsl': (
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) { return saturate(base + 2.0*blend - 1.0); }\n' +
        BLEND_FOOTER
    ),
    'CS_BlendHardMix.hlsl': (
        '#include "CS_BlendVividLight.hlsl"\n' +
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) { return step(0.5, VividLight(base, blend)); }\n' +
        BLEND_FOOTER
    ),
    'CS_BlendDissolve.hlsl': (
        BLEND_HEADER +
        'float hash(uint2 p) { return frac(sin(dot(float2(p), float2(12.9898,78.233)))*43758.5453); }\n'
        '[numthreads(8,8,1)]\n'
        'void main(uint3 DTid : SV_DispatchThreadID)\n'
        '{\n'
        '    float4 src = SrcTex.Load(int3(DTid.xy, 0));\n'
        '    float4 dst = DstTex.Load(int3(DTid.xy, 0));\n'
        '    float rnd = hash(DTid.xy);\n'
        '    float3 result = (rnd < src.a) ? src.rgb : dst.rgb;\n'
        '    ResultTex[DTid.xy] = float4(result, 1.0);\n'
        '}\n'
    ),
    'CS_BlendDancingDissolve.hlsl': (
        'cbuffer DissolveParams : register(b2) { float2 _pad; float frameSeed; }\n' +
        BLEND_HEADER +
        'float hash(float2 p) { return frac(sin(dot(p, float2(12.9898,78.233)))*43758.5453); }\n'
        '[numthreads(8,8,1)]\n'
        'void main(uint3 DTid : SV_DispatchThreadID)\n'
        '{\n'
        '    float4 src = SrcTex.Load(int3(DTid.xy, 0));\n'
        '    float4 dst = DstTex.Load(int3(DTid.xy, 0));\n'
        '    float rnd = hash(float2(DTid.xy) + frameSeed);\n'
        '    float3 result = (rnd < src.a) ? src.rgb : dst.rgb;\n'
        '    ResultTex[DTid.xy] = float4(result, 1.0);\n'
        '}\n'
    ),
    'CS_BlendClassicColorBurn.hlsl': (
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) { return 1.0-(1.0-base)/max(blend,1e-5); }\n' +
        BLEND_FOOTER
    ),
    'CS_BlendLinearDodge.hlsl': (
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) { return saturate(base + blend); }\n' +
        BLEND_FOOTER
    ),
    'CS_BlendClassicColorDodge.hlsl': (
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) { return min(base / max(blend, 1e-5), 1.0); }\n' +
        BLEND_FOOTER
    ),
    'CS_BlendClassicDifference.hlsl': (
        BLEND_HEADER +
        'float3 BlendFunc(float3 base, float3 blend) { return abs(base - blend); }\n' +
        BLEND_FOOTER
    ),
}

for name, body in shaders.items():
    path = os.path.join(base, name)
    with open(path, 'w', newline='') as f:
        f.write(body)
    print(f'Created: {name[11:-5]} ({name})')

print(f'\nTotal: {len(shaders)} GPU shaders created')
