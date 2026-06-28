#include "pch.h"
#include "UIRenderer.h"
#include "UIFontManager.h"

void UIRenderer::initialize()
{
    if (m_initialized) return;

    auto* device = DX12::Instance().getDevice();

    // ── 頂点バッファ（動的 Upload ヒープ、マップ常時保持）───────────────
    {
        const UINT64 vbSize = sizeof(UIVertex) * k_maxVertices;
        const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC   resDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_vertexBuffer));
        assert(SUCCEEDED(hr));
        m_vertexBuffer->SetName(L"UIRenderer::VertexBuffer");

        m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedVerts));

        m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vbView.StrideInBytes  = static_cast<UINT>(sizeof(UIVertex));
        m_vbView.SizeInBytes    = static_cast<UINT>(vbSize);
    }

    // ── インデックスバッファ（静的、全クワッド共有）───────────────────────
    {
        // 各クワッドの三角形インデックス: [0,1,2] [0,2,3]
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
        assert(SUCCEEDED(hr));
        m_indexBuffer->SetName(L"UIRenderer::IndexBuffer");

        void* mapped = nullptr;
        m_indexBuffer->Map(0, nullptr, &mapped);
        memcpy(mapped, indices.data(), static_cast<size_t>(ibSize));
        m_indexBuffer->Unmap(0, nullptr);

        m_ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_ibView.Format         = DXGI_FORMAT_R32_UINT;
        m_ibView.SizeInBytes    = static_cast<UINT>(ibSize);
    }

    // ── 定数バッファ（デスクリプタヒープ不要の CBV リングバッファ）─────────────────
    {
        // Root CBV は GPU 仮想アドレスバインド。ヒープエントリ不要。
        m_cbStride = (static_cast<UINT>(sizeof(UIConstantData)) + 255u) & ~255u;
        const UINT64 cbSize = static_cast<UINT64>(m_cbStride) * k_maxCBSlots;

        const CD3DX12_HEAP_PROPERTIES cbHeap(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC   cbDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

        HRESULT hr = device->CreateCommittedResource(
            &cbHeap, D3D12_HEAP_FLAG_NONE, &cbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_cbRingBuffer));
        assert(SUCCEEDED(hr));
        m_cbRingBuffer->SetName(L"UIRenderer::CBRingBuffer");
        m_cbRingBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_cbMapped));
    }

    // ── ホワイトテクスチャ────────────────────────────────────────────────
    createWhiteTexture();

    // ── PSO 登録──────────────────────────────────────────────────────────
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
//  フレーム制御
// =============================================================
void UIRenderer::begin(float screenWidth, float screenHeight)
{
    m_screenWidth  = screenWidth;
    m_screenHeight = screenHeight;
    m_cbSlot       = 0;
    m_vertices.clear();
    m_drawCommands.clear();

    // 正射影行列: (0,0)=左上 → NDC(-1,+1), (screenW, screenH)=右下 → NDC(+1,-1)
    m_orthoMatrix = Matrix::CreateOrthographicOffCenter(
        0.f, screenWidth, screenHeight, 0.f, 0.f, 1.f);
}

void UIRenderer::end()
{
    if (!m_initialized || m_drawCommands.empty()) return;

    // CPU バッファ → GPU マップへコピー
    const UINT vertCount = static_cast<UINT>(m_vertices.size());
    assert(vertCount <= k_maxVertices);
    memcpy(m_mappedVerts, m_vertices.data(), sizeof(UIVertex) * vertCount);

    auto* cmd = DX12::Instance().getGraphicsCommandList();

    // ★修正: リソースバリア - シーン RT を PIXEL_SHADER_RESOURCE → RENDER_TARGET に遷移
    ID3D12Resource* sceneRT = DX12::Instance().getSceneRenderTarget();
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = sceneRT;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &barrier);

    // シーン RT を描画ターゲットとして設定
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = DX12::Instance().getSceneRTVHandle();
    cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    // ビューポートとシザーを画面サイズに合わせる
    D3D12_VIEWPORT vp = { 0.f, 0.f, m_screenWidth, m_screenHeight, 0.f, 1.f };
    D3D12_RECT     sr = { 0, 0, static_cast<LONG>(m_screenWidth), static_cast<LONG>(m_screenHeight) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sr);

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);

    flushCommands(cmd);

    // ★修正: リソースバリア - UI 描画完了後、シーン RT を RENDER_TARGET → PIXEL_SHADER_RESOURCE に遷移
    // （次のフレームで posteffect や他の処理で使用するため）
    D3D12_RESOURCE_BARRIER barrierEnd = {};
    barrierEnd.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierEnd.Transition.pResource = sceneRT;
    barrierEnd.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierEnd.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrierEnd.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &barrierEnd);
}

// =============================================================
//  描画 API
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
    //! ワールド空間: Canvas のワールド行列 * VP を transform として使用
    const Matrix mvp = worldTransform * viewProjection;

    //! ローカル座標は中心原点 [-w/2, w/2] x [-h/2, h/2]
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
//  内部実装
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

    // 4 頂点をプッシュ（左上→右上→右下→左下）
    m_vertices.push_back({ {x,     y    }, {u0, v0}, color });
    m_vertices.push_back({ {x + w, y    }, {u1, v0}, color });
    m_vertices.push_back({ {x + w, y + h}, {u1, v1}, color });
    m_vertices.push_back({ {x,     y + h}, {u0, v1}, color });

    // 定数バッファにアップロード
    UIConstantData cbData{};
    cbData.transform      = worldTransform;
    cbData.localTransform = localTransform;
    cbData.tintColor      = Vector4(1, 1, 1, 1);
    cbData.textureMode    = textureMode;
    cbData.globalAlpha    = alpha;

    const UINT cbSlot = uploadConstants(cbData);

    // 既存コマンドと同一条件ならマージ（バッチ最適化）
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
    // スロットが満杯なら末尾を上書き（フレーム内で過剰な描画がある場合の安全策）
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
        // CBV (b0) —— Root CBV: GPU 仮想アドレスを直接設定（デスクリプタヒープ不要）
        cmd->SetGraphicsRootConstantBufferView(
            0, getCBGPUAddress(dc.cbSlot));

        // SRV テーブル (t0)
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
    //! 1x1 白ピクセル RGBA
    constexpr uint32_t white = 0xFFFFFFFF;
    m_whiteTexture = std::make_unique<LoadTexture>(
        1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &white, sizeof(white));
    m_whiteSrvIndex = m_whiteTexture->getSRVIndex();
}
