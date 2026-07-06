#ifndef MATERIAL_GRAPH_GENERATED_HLSLI
#define MATERIAL_GRAPH_GENERATED_HLSLI

struct MaterialGraphResult
{
    float4 baseColor;
    float metallic;
    float roughness;
    float ao;
    float alpha;
    float3 normalTS;
    float hasNormal;
};

MaterialGraphResult EvaluateMaterialGraph_0(
    float2 uv,
    float4 materialDiffuse,
    float3 materialPbr,
    Texture2D<float4> baseColorTex,
    Texture2D<float4> normalTex,
    SamplerState linearSampler)
{
    MaterialGraphResult outValue;
    float4 sampledColor = baseColorTex.Sample(linearSampler, uv);
    float3 sampledNormal = normalize(normalTex.Sample(linearSampler, uv).xyz * 2.0f - 1.0f);
    outValue.baseColor = sampledColor * materialDiffuse;
    outValue.metallic = saturate(materialPbr.x);
    outValue.roughness = saturate(materialPbr.y);
    outValue.ao = saturate(materialPbr.z);
    outValue.alpha = saturate(outValue.baseColor.a);
    outValue.normalTS = sampledNormal;
    outValue.hasNormal = 1.0f;
    return outValue;
}

MaterialGraphResult EvaluateMaterialGraphById(
    int graphId,
    float2 uv,
    float4 materialDiffuse,
    float3 materialPbr,
    Texture2D<float4> baseColorTex,
    Texture2D<float4> normalTex,
    SamplerState linearSampler)
{
    switch (graphId)
    {
    case 0:
        return EvaluateMaterialGraph_0(uv, materialDiffuse, materialPbr, baseColorTex, normalTex, linearSampler);
    default:
        return EvaluateMaterialGraph_0(uv, materialDiffuse, materialPbr, baseColorTex, normalTex, linearSampler);
    }
}

// Compatibility wrappers for domain-specific graph entry points.
MaterialGraphResult EvaluateSurfaceGraphById(
    int graphId,
    float2 uv,
    float4 materialDiffuse,
    float3 materialPbr,
    Texture2D<float4> baseColorTex,
    Texture2D<float4> normalTex,
    SamplerState linearSampler)
{
    return EvaluateMaterialGraphById(graphId, uv, materialDiffuse, materialPbr, baseColorTex, normalTex, linearSampler);
}

MaterialGraphResult EvaluateParticleGraphById(
    int graphId,
    float2 uv,
    float4 materialDiffuse,
    float3 materialPbr,
    Texture2D<float4> baseColorTex,
    Texture2D<float4> normalTex,
    SamplerState linearSampler)
{
    return EvaluateMaterialGraphById(graphId, uv, materialDiffuse, materialPbr, baseColorTex, normalTex, linearSampler);
}

MaterialGraphResult EvaluatePostEffectGraphById(
    int graphId,
    float2 uv,
    float4 materialDiffuse,
    float3 materialPbr,
    Texture2D<float4> baseColorTex,
    Texture2D<float4> normalTex,
    SamplerState linearSampler)
{
    return EvaluateMaterialGraphById(graphId, uv, materialDiffuse, materialPbr, baseColorTex, normalTex, linearSampler);
}

#endif
