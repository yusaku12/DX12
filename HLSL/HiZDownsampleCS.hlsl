cbuffer HiZParams : register(b0)
{
    uint srcWidth;
    uint srcHeight;
    uint isFirstPass;
    uint _padding;
}

Texture2D<float> gSrcDepth : register(t0);
RWTexture2D<float> gDstMip : register(u0);

[numthreads(8, 8, 1)]
void CS(uint3 dtid : SV_DispatchThreadID)
{
    uint dstWidth, dstHeight;
    gDstMip.GetDimensions(dstWidth, dstHeight);

    if (dtid.x >= dstWidth || dtid.y >= dstHeight)
    {
        return;
    }

    if (isFirstPass != 0)
    {
        const uint2 uv = uint2(min(dtid.x, srcWidth - 1), min(dtid.y, srcHeight - 1));
        const float z = gSrcDepth.Load(int3(uv, 0));
        gDstMip[dtid.xy] = z;
        return;
    }

    const uint2 baseUv = dtid.xy * 2;

    const uint2 uv00 = uint2(min(baseUv.x + 0, srcWidth - 1), min(baseUv.y + 0, srcHeight - 1));
    const uint2 uv10 = uint2(min(baseUv.x + 1, srcWidth - 1), min(baseUv.y + 0, srcHeight - 1));
    const uint2 uv01 = uint2(min(baseUv.x + 0, srcWidth - 1), min(baseUv.y + 1, srcHeight - 1));
    const uint2 uv11 = uint2(min(baseUv.x + 1, srcWidth - 1), min(baseUv.y + 1, srcHeight - 1));

    const float d0 = gSrcDepth.Load(int3(uv00, 0));
    const float d1 = gSrcDepth.Load(int3(uv10, 0));
    const float d2 = gSrcDepth.Load(int3(uv01, 0));
    const float d3 = gSrcDepth.Load(int3(uv11, 0));

    // Depth range is 0:near / 1:far. Use max for conservative Hi-Z.
    gDstMip[dtid.xy] = max(max(d0, d1), max(d2, d3));
}
