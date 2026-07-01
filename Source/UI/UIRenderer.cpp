#include "pch.h"
#include "UIRenderer.h"
#include "UIFontManager.h"

void UIRenderer::initialize()
{
    if (m_initialized) return;

    auto* device = DX12::Instance().getDevice();

    // 笏笏 鬆らせ繝舌ャ繝輔ぃ・亥虚逧・Upload 繝偵・繝励√・繝・・蟶ｸ譎ゆｿ晄戟・俄楳笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    {
        const UINT64 vbSize = sizeof(UIVertex) * k_maxVertices;
        const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC   resDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_vertexBuffer));
        if (FAILED(hr))
        {
            LOG_ERROR("UIRenderer: failed to create vertex buffer");
            return;
        }
        m_vertexBuffer->SetName(L"UIRenderer::VertexBuffer");

        m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedVerts));

        m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vbView.StrideInBytes  = static_cast<UINT>(sizeof(UIVertex));
        m_vbView.SizeInBytes    = static_cast<UINT>(vbSize);
    }

    // 笏笏 繧､繝ｳ繝・ャ繧ｯ繧ｹ繝舌ャ繝輔ぃ・磯撕逧・∝・繧ｯ繝ｯ繝・ラ蜈ｱ譛会ｼ俄楳笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    {
        // 蜷・け繝ｯ繝・ラ縺ｮ荳芽ｧ貞ｽ｢繧､繝ｳ繝・ャ繧ｯ繧ｹ: [0,1,2] [0,2,3]
        std::vector<uint32_t> indices(k_maxQuads * 6);
        for (UINT q = 0; q < k_maxQuads; ++q)
        {
            const UINT base = q * 4;
            const UINT off  = q * 6;
            indices[off + 0] = base + 0;
            indices[off + 1] = base + 1;
            indices[off + 2] = base + 2;
            indices[off + 3] = base + 0;
            indices[off + 4] = base + 2;
            indices[off + 5] = base + 3;
        }

        const UINT64 ibSize = sizeof(uint32_t) * indices.size();
        const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC   resDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_indexBuffer));
        if (FAILED(hr))
        {
            LOG_ERROR("UIRenderer: failed to create index buffer");
            return;
        }
        m_indexBuffer->SetName(L"UIRenderer::IndexBuffer");

        void* mapped = nullptr;
        m_indexBuffer->Map(0, nullptr, &mapped);
        memcpy(mapped, indices.data(), static_cast<size_t>(ibSize));
        m_indexBuffer->Unmap(0, nullptr);

        m_ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_ibView.Format         = DXGI_FORMAT_R32_UINT;
        m_ibView.SizeInBytes    = static_cast<UINT>(ibSize);
    }

    // 笏笏 螳壽焚繝舌ャ繝輔ぃ・医ョ繧ｹ繧ｯ繝ｪ繝励ち繝偵・繝嶺ｸ崎ｦ√・ CBV 繝ｪ繝ｳ繧ｰ繝舌ャ繝輔ぃ・俄楳笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    {
        // Root CBV 縺ｯ GPU 莉ｮ諠ｳ繧｢繝峨Ξ繧ｹ繝舌う繝ｳ繝峨ゅヲ繝ｼ繝励お繝ｳ繝医Μ荳崎ｦ√・
        m_cbStride = (static_cast<UINT>(sizeof(UIConstantData)) + 255u) & ~255u;
        const UINT64 cbSize = static_cast<UINT64>(m_cbStride) * k_maxCBSlots;

        const CD3DX12_HEAP_PROPERTIES cbHeap(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC   cbDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

        HRESULT hr = device->CreateCommittedResource(
            &cbHeap, D3D12_HEAP_FLAG_NONE, &cbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_cbRingBuffer));
        if (FAILED(hr))
        {
            LOG_ERROR("UIRenderer: failed to create constant buffer ring");
            return;
        }
        m_cbRingBuffer->SetName(L"UIRenderer::CBRingBuffer");
        m_cbRingBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_cbMapped));
    }

    // 笏笏 繝帙Ρ繧､繝医ユ繧ｯ繧ｹ繝√Ε笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    createWhiteTexture();

    // 笏笏 PSO 逋ｻ骭ｲ笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    {
        static const D3D12_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        PSOCreator::PSOData psoData;
        psoData.rootSignatureType = RootSignatureType::UI;
        psoData.vsShaderId        = ShaderID::UIVS;
        psoData.psShaderId        = ShaderID::UIPS;
        psoData.rasterizerState   = RasterizerState::CULL_NONE;
        psoData.blendState        = BlendState::ALPHA;
        psoData.depthStencilState = DepthStencilState::DEPTH_NONE;
        psoData.topologyType      = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoData.inputLayout       = { std::begin(layout), std::end(layout) };
        psoData.numRenderTargets  = 1;
        psoData.rtvFormats[0]     = DX12::Instance().getBackBufferFormat();

        m_psoKey = PSOCreator::Instance().registerPSO(psoData);
    }

    m_vertices.reserve(k_maxVertices);
    m_drawCommands.reserve(512);
    m_initialized = true;

    LOG_INFO("UIRenderer: initialized");
}

void UIRenderer::shutdown()
{
    if (!m_initialized) return;

    if (m_mappedVerts)
    {
        m_vertexBuffer->Unmap(0, nullptr);
        m_mappedVerts = nullptr;
    }

    if (m_cbMapped)
    {
        m_cbRingBuffer->Unmap(0, nullptr);
        m_cbMapped = nullptr;
    }
    m_cbRingBuffer.Reset();
    m_whiteTexture.reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_initialized = false;
}

// =============================================================
//  繝輔Ξ繝ｼ繝蛻ｶ蠕｡
// =============================================================
void UIRenderer::begin(float screenWidth, float screenHeight)
{
    m_screenWidth  = screenWidth;
    m_screenHeight = screenHeight;
    m_cbSlot       = 0;
    m_vertices.clear();
    m_drawCommands.clear();

    // 豁｣蟆・ｽｱ陦悟・: (0,0)=蟾ｦ荳・竊・NDC(-1,+1), (screenW, screenH)=蜿ｳ荳・竊・NDC(+1,-1)
    m_orthoMatrix = Matrix::CreateOrthographicOffCenter(
        0.f, screenWidth, screenHeight, 0.f, 0.f, 1.f);
}

void UIRenderer::end()
{
    if (!m_initialized || m_drawCommands.empty()) return;

    // CPU 繝舌ャ繝輔ぃ 竊・GPU 繝槭ャ繝励∈繧ｳ繝斐・
    const UINT vertCount = static_cast<UINT>(m_vertices.size());
    assert(vertCount <= k_maxVertices);
    memcpy(m_mappedVerts, m_vertices.data(), sizeof(UIVertex) * vertCount);

    auto* cmd = DX12::Instance().getGraphicsCommandList();

    // 笘・ｿｮ豁｣: 繝ｪ繧ｽ繝ｼ繧ｹ繝舌Μ繧｢ - 繧ｷ繝ｼ繝ｳ RT 繧・PIXEL_SHADER_RESOURCE 竊・RENDER_TARGET 縺ｫ驕ｷ遘ｻ
    ID3D12Resource* sceneRT = DX12::Instance().getSceneRenderTarget();
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = sceneRT;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &barrier);

    // 繧ｷ繝ｼ繝ｳ RT 繧呈緒逕ｻ繧ｿ繝ｼ繧ｲ繝・ヨ縺ｨ縺励※險ｭ螳・
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = DX12::Instance().getSceneRTVHandle();
    cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    // 繝薙Η繝ｼ繝昴・繝医→繧ｷ繧ｶ繝ｼ繧堤判髱｢繧ｵ繧､繧ｺ縺ｫ蜷医ｏ縺帙ｋ
    D3D12_VIEWPORT vp = { 0.f, 0.f, m_screenWidth, m_screenHeight, 0.f, 1.f };
    D3D12_RECT     sr = { 0, 0, static_cast<LONG>(m_screenWidth), static_cast<LONG>(m_screenHeight) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sr);

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);

    flushCommands(cmd);

    // 笘・ｿｮ豁｣: 繝ｪ繧ｽ繝ｼ繧ｹ繝舌Μ繧｢ - UI 謠冗判螳御ｺ・ｾ後√す繝ｼ繝ｳ RT 繧・RENDER_TARGET 竊・PIXEL_SHADER_RESOURCE 縺ｫ驕ｷ遘ｻ
    // ・域ｬ｡縺ｮ繝輔Ξ繝ｼ繝縺ｧ posteffect 繧・ｻ悶・蜃ｦ逅・〒菴ｿ逕ｨ縺吶ｋ縺溘ａ・・
    D3D12_RESOURCE_BARRIER barrierEnd = {};
    barrierEnd.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierEnd.Transition.pResource = sceneRT;
    barrierEnd.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierEnd.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrierEnd.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &barrierEnd);
}

// =============================================================
//  謠冗判 API
// =============================================================
void UIRenderer::drawRect(float x, float y, float w, float h,
                          const Vector4& color,
                          const Matrix* localTransform, float alpha)
{
    pushQuad(x, y, w, h,
             0.f, 0.f, 1.f, 1.f,
             color,
             m_whiteSrvIndex, 0u,
             m_orthoMatrix,
             localTransform ? *localTransform : Matrix::Identity,
             alpha * m_globalAlpha);
}

void UIRenderer::drawTexturedRect(float x, float y, float w, float h,
                                  UINT srvIndex, const Vector4& tintColor,
                                  const Matrix* localTransform, float alpha)
{
    pushQuad(x, y, w, h,
             0.f, 0.f, 1.f, 1.f,
             tintColor,
             srvIndex, 1u,
             m_orthoMatrix,
             localTransform ? *localTransform : Matrix::Identity,
             alpha * m_globalAlpha);
}

float UIRenderer::drawText(float x, float y,
                           const std::string& text,
                           const Vector4& color,
                           float scale,
                           const Matrix* localTransform, float alpha)
{
    if (!UIFontManager::Instance().isInitialized()) return x;

    float cursorX = x;
    const UINT fontSrv = UIFontManager::Instance().getAtlasSrvIndex();
    const Matrix localMat = localTransform ? *localTransform : Matrix::Identity;

    for (unsigned char ch : text)
    {
        const UIGlyphInfo* glyph = UIFontManager::Instance().getGlyph(ch);
        if (!glyph) { cursorX += 8.f * scale; continue; }

        const float gx = cursorX + glyph->bearingX * scale;
        const float gy = y - glyph->bearingY * scale;
        const float gw = glyph->width  * scale;
        const float gh = glyph->height * scale;

        if (gw > 0.f && gh > 0.f)
        {
            pushQuad(gx, gy, gw, gh,
                     glyph->uv0.x, glyph->uv0.y,
                     glyph->uv1.x, glyph->uv1.y,
                     color,
                     fontSrv, 2u,
                     m_orthoMatrix,
                     localMat,
                     alpha * m_globalAlpha);
        }

        cursorX += glyph->advance * scale;
    }

    return cursorX;
}

Vector2 UIRenderer::measureText(const std::string& text, float scale) const
{
    return UIFontManager::Instance().measureText(text, scale);
}

void UIRenderer::drawWorldRect(const Matrix& worldTransform,
                               const Matrix& viewProjection,
                               float w, float h,
                               const Vector4& color, float alpha)
{
    //! 繝ｯ繝ｼ繝ｫ繝臥ｩｺ髢・ Canvas 縺ｮ繝ｯ繝ｼ繝ｫ繝芽｡悟・ * VP 繧・transform 縺ｨ縺励※菴ｿ逕ｨ
    const Matrix mvp = worldTransform * viewProjection;

    //! 繝ｭ繝ｼ繧ｫ繝ｫ蠎ｧ讓吶・荳ｭ蠢・次轤ｹ [-w/2, w/2] x [-h/2, h/2]
    const float hw = w * 0.5f;
    const float hh = h * 0.5f;

    pushQuad(-hw, -hh, w, h,
             0.f, 0.f, 1.f, 1.f,
             color,
             m_whiteSrvIndex, 0u,
             mvp,
             Matrix::Identity,
             alpha * m_globalAlpha);
}

UINT UIRenderer::getFontAtlasSrvIndex() const
{
    return UIFontManager::Instance().getAtlasSrvIndex();
}

// =============================================================
//  蜀・Κ螳溯｣・
// =============================================================
void UIRenderer::pushQuad(float x, float y, float w, float h,
                          float u0, float v0, float u1, float v1,
                          const Vector4& color,
                          UINT srvIndex, UINT textureMode,
                          const Matrix& worldTransform,
                          const Matrix& localTransform,
                          float alpha)
{
    if (m_vertices.size() + 4 > k_maxVertices) return;

    // 4 鬆らせ繧偵・繝・す繝･・亥ｷｦ荳岩・蜿ｳ荳岩・蜿ｳ荳銀・蟾ｦ荳具ｼ・
    m_vertices.push_back({ {x,     y    }, {u0, v0}, color });
    m_vertices.push_back({ {x + w, y    }, {u1, v0}, color });
    m_vertices.push_back({ {x + w, y + h}, {u1, v1}, color });
    m_vertices.push_back({ {x,     y + h}, {u0, v1}, color });

    // 螳壽焚繝舌ャ繝輔ぃ縺ｫ繧｢繝・・繝ｭ繝ｼ繝・
    UIConstantData cbData{};
    cbData.transform      = worldTransform;
    cbData.localTransform = localTransform;
    cbData.tintColor      = Vector4(1, 1, 1, 1);
    cbData.textureMode    = textureMode;
    cbData.globalAlpha    = alpha;

    const UINT cbSlot = uploadConstants(cbData);

    // 譌｢蟄倥さ繝槭Φ繝峨→蜷御ｸ譚｡莉ｶ縺ｪ繧峨・繝ｼ繧ｸ・医ヰ繝・メ譛驕ｩ蛹厄ｼ・
    if (!m_drawCommands.empty())
    {
        auto& last = m_drawCommands.back();
        if (last.srvIndex    == srvIndex    &&
            last.textureMode == textureMode &&
            last.cbSlot      == cbSlot)
        {
            last.quadCount++;
            return;
        }
    }

    UIDrawCommand cmd{};
    cmd.startVertex = static_cast<UINT>(m_vertices.size()) - 4;
    cmd.quadCount   = 1;
    cmd.srvIndex    = srvIndex;
    cmd.textureMode = textureMode;
    cmd.cbSlot      = cbSlot;
    m_drawCommands.push_back(cmd);
}

UINT UIRenderer::uploadConstants(const UIConstantData& data)
{
    // 繧ｹ繝ｭ繝・ヨ縺梧ｺ譚ｯ縺ｪ繧画忰蟆ｾ繧剃ｸ頑嶌縺搾ｼ医ヵ繝ｬ繝ｼ繝蜀・〒驕主臆縺ｪ謠冗判縺後≠繧句ｴ蜷医・螳牙・遲厄ｼ・
    const UINT slot = (m_cbSlot < k_maxCBSlots) ? m_cbSlot++ : k_maxCBSlots - 1;
    memcpy(m_cbMapped + static_cast<size_t>(slot) * m_cbStride, &data, sizeof(data));
    return slot;
}

D3D12_GPU_VIRTUAL_ADDRESS UIRenderer::getCBGPUAddress(UINT slot) const
{
    return m_cbRingBuffer->GetGPUVirtualAddress()
         + static_cast<UINT64>(slot) * m_cbStride;
}

void UIRenderer::flushCommands(ID3D12GraphicsCommandList* cmd)
{
    PSOCreator::Instance().setPSO(m_psoKey, cmd);
    cmd->SetGraphicsRootSignature(
        RootSignatureManager::Instance().getRootSignature(RootSignatureType::UI));
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &m_vbView);
    cmd->IASetIndexBuffer(&m_ibView);

    for (const auto& dc : m_drawCommands)
    {
        // CBV (b0) 窶披・Root CBV: GPU 莉ｮ諠ｳ繧｢繝峨Ξ繧ｹ繧堤峩謗･險ｭ螳夲ｼ医ョ繧ｹ繧ｯ繝ｪ繝励ち繝偵・繝嶺ｸ崎ｦ・ｼ・
        cmd->SetGraphicsRootConstantBufferView(
            0, getCBGPUAddress(dc.cbSlot));

        // SRV 繝・・繝悶Ν (t0)
        const UINT srvIdx = (dc.srvIndex == UINT_MAX) ? m_whiteSrvIndex : dc.srvIndex;
        cmd->SetGraphicsRootDescriptorTable(
            1, DescriptorHeapManager::Instance().getGPUHandle(srvIdx));

        const UINT startIndex  = (dc.startVertex / 4) * 6;
        const UINT indexCount  = dc.quadCount * 6;
        cmd->DrawIndexedInstanced(indexCount, 1, startIndex, 0, 0);
    }
}

void UIRenderer::createWhiteTexture()
{
    //! 1x1 逋ｽ繝斐け繧ｻ繝ｫ RGBA
    constexpr uint32_t white = 0xFFFFFFFF;
    m_whiteTexture = DXMem::makeUnique<LoadTexture>(
        1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &white, sizeof(white));
    m_whiteSrvIndex = m_whiteTexture->getSRVIndex();
}
