#include "PostEffect.hlsli"

//! フルスクリーン三角形（頂点バッファ不要）
//! SV_VertexID 0,1,2 から画面全体をカバーする三角形を生成
PostEffectVSOut VS(uint vertexID : SV_VertexID)
{
    PostEffectVSOut output;

    //! 大きな三角形で画面全体をカバー
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.svpos = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

    return output;
}
