#include "pch.h"
#include "Render/ShadowMapRenderer.h"
#include "Camera/CameraManager.h"
#include "Camera/CameraComponent.h"

//=====================================================
// ShadowMapRenderer 実装
// Cascaded Shadow Maps (CSM) ? Unity / Unreal 準拠
//=====================================================

void ShadowMapRenderer::initialize()
{
    createResources();

    // 定数バッファ生成
    for (int i = 0; i < CascadeCount; ++i)
    {
        m_lightVPCBs[i] = DXMem::makeUnique<ConstantBuffer<ShadowLightCB>>();
    }
    m_shadowParamsCB = DXMem::makeUnique<ConstantBuffer<ShadowParamsCB>>();

    LOG_INFO("ShadowMapRenderer: initialized (CSM 4 cascades, 2048x2048)");
}

//-----------------------------------------------------
// リソース生成（テクスチャ、DSV、SRV）
//-----------------------------------------------------
void ShadowMapRenderer::createResources()
{
    auto device = DX12::Instance().getDevice();

    // Texture2DArray（DXGI_FORMAT_D32_FLOAT、CascadeCount スライス）
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = ShadowMapSize;
    texDesc.Height = ShadowMapSize;
    texDesc.DepthOrArraySize = static_cast<UINT16>(CascadeCount);
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_D32_FLOAT;
    texDesc.SampleDesc = { 1, 0 };
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearVal = {};
    clearVal.Format = DXGI_FORMAT_D32_FLOAT;
    clearVal.DepthStencil.Depth = 1.0f;
    clearVal.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearVal,
        IID_PPV_ARGS(&m_shadowTexture)
    );
    assert(SUCCEEDED(hr));
    m_shadowTexture->SetName(L"ShadowMapTexture");
    m_textureState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    // DSV ヒープ（CPU 専用、シェーダー不可視）
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = CascadeCount;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));
    assert(SUCCEEDED(hr));

    const UINT dsvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    for (int i = 0; i < CascadeCount; ++i)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
        dsvHandle.ptr += static_cast<SIZE_T>(i) * dsvIncrement;
        m_dsvHandles[i] = dsvHandle;

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = static_cast<UINT>(i);
        dsvDesc.Texture2DArray.ArraySize = 1;

        device->CreateDepthStencilView(m_shadowTexture.Get(), &dsvDesc, dsvHandle);
    }

    // SRV（DescriptorHeapManager 経由でシェーダー可視ヒープに登録）
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = CascadeCount;
    srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

    m_srvIndex = DescriptorHeapManager::Instance().createSRV(m_shadowTexture.Get(), srvDesc);
}

//-----------------------------------------------------
// フレーム更新：カスケード計算
//-----------------------------------------------------
void ShadowMapRenderer::update(const Vector3& lightDir)
{
    m_lightDir = lightDir;
    computeCascades();
    m_shadowParamsCB->update(m_shadowParams);
}

//-----------------------------------------------------
// カスケード分割と光源行列の計算
//-----------------------------------------------------
void ShadowMapRenderer::computeCascades()
{
    CameraComponent* cam = CameraManager::Instance().getMainCamera();
    if (!cam) return;

    const Matrix view = cam->getView();
    const Matrix proj = cam->getProjection();
    const float  nearZ = cam->getNear();
    const float  farZ = cam->getFar();

    // VP 逆行列でフラスタムコーナーをワールド空間へ
    const Matrix invVP = (view * proj).Invert();

    // DX12 NDC: near=z0, far=z1
    static const Vector3 s_ndcCorners[8] =
    {
        {-1,+1,0},{+1,+1,0},{+1,-1,0},{-1,-1,0},  //!< near plane
        {-1,+1,1},{+1,+1,1},{+1,-1,1},{-1,-1,1},  //!< far plane
    };

    Vector3 worldCorners[8];
    for (int j = 0; j < 8; ++j)
    {
        Vector4 v = Vector4::Transform(
            Vector4(s_ndcCorners[j].x, s_ndcCorners[j].y, s_ndcCorners[j].z, 1.0f),
            invVP);
        worldCorners[j] = Vector3(v.x / v.w, v.y / v.w, v.z / v.w);
    }

    // λ ブレンドカスケード分割（対数 × λ + 一様 × (1-λ)）
    float splits[CascadeCount];
    for (int i = 0; i < CascadeCount; ++i)
    {
        const float ratio = farZ / nearZ;
        const float logSplit = nearZ * std::pow(ratio, (i + 1.0f) / CascadeCount);
        const float uniformSplit = nearZ + (farZ - nearZ) * (i + 1.0f) / CascadeCount;
        splits[i] = CascadeLambda * logSplit + (1.0f - CascadeLambda) * uniformSplit;
    }

    // 光源方向を正規化
    Vector3 normDir = m_lightDir;
    normDir.Normalize();

    for (int i = 0; i < CascadeCount; ++i)
    {
        const float prevSplit = (i == 0) ? nearZ : splits[i - 1];
        const float currSplit = splits[i];

        // フラスタム near/far ray を t でブレンドしてサブフラスタムコーナーを取得
        const float tPrev = (prevSplit - nearZ) / (farZ - nearZ);
        const float tCurr = (currSplit - nearZ) / (farZ - nearZ);

        Vector3 cascadeCorners[8];
        for (int j = 0; j < 4; ++j)
        {
            const Vector3 ray = worldCorners[j + 4] - worldCorners[j];
            cascadeCorners[j] = worldCorners[j] + ray * tPrev;
            cascadeCorners[j + 4] = worldCorners[j] + ray * tCurr;
        }

        // サブフラスタム中心
        Vector3 center = Vector3::Zero;
        for (auto& c : cascadeCorners) center += c;
        center /= 8.0f;

        // 光源ビュー行列（中心から光源方向を向く）
        const Matrix lightView = buildLightView(center, normDir);

        // バウンディング球半径（安定した固定サイズ → シマーなし）
        float maxRadius = 0.0f;
        for (auto& c : cascadeCorners)
            maxRadius = std::max(maxRadius, Vector3::Distance(center, c));

        maxRadius = std::ceil(maxRadius);  //!< 浮動小数点誤差対策で切り上げ

        // テクセルスナッピング（カメラ移動時のシマーを防ぐ）
        const float texelSize = 2.0f * maxRadius / static_cast<float>(ShadowMapSize);
        Vector3     lsCenter = Vector3::Transform(center, lightView);
        lsCenter.x = std::floor(lsCenter.x / texelSize) * texelSize;
        lsCenter.y = std::floor(lsCenter.y / texelSize) * texelSize;

        // 正射影行列（minZ を -50 延長してカメラ背後のシャドウキャスターをカバー）
        const Matrix lightProj = Matrix::CreateOrthographicOffCenter(
            lsCenter.x - maxRadius,
            lsCenter.x + maxRadius,
            lsCenter.y - maxRadius,
            lsCenter.y + maxRadius,
            lsCenter.z - (maxRadius + 50.0f),
            lsCenter.z + maxRadius
        );

        const Matrix lightVP = lightView * lightProj;

        // カスケード OBB の更新 (ライトスペースの AABB を定義し、ワールド空間へ逆変換)
        m_cascadeOBBs[i].Center = Vector3(lsCenter.x, lsCenter.y, lsCenter.z - 25.0f);
        m_cascadeOBBs[i].Extents = Vector3(maxRadius, maxRadius, maxRadius + 25.0f);
        m_cascadeOBBs[i].Orientation = Quaternion::Identity;
        m_cascadeOBBs[i].Transform(m_cascadeOBBs[i], lightView.Invert());

        // パラメータ更新
        m_shadowParams.lightViewProj[i] = lightVP;
        reinterpret_cast<float*>(&m_shadowParams.cascadeSplits)[i] = currSplit;

        // カスケードごとの CBV 更新
        ShadowLightCB lightCB;
        lightCB.lightViewProj = lightVP;
        lightCB.cascadeIndex = static_cast<float>(i);
        m_lightVPCBs[i]->update(lightCB);
    }
}

//-----------------------------------------------------
// 光源ビュー行列ビルダー
//-----------------------------------------------------
Matrix ShadowMapRenderer::buildLightView(const Vector3& center, const Vector3& lightDir)
{
    // 光源はシーン中心から光方向の逆向きに配置
    const Vector3 eye = center - lightDir * 200.0f;

    // 上ベクトル（光源方向と平行にならないよう選択）
    const Vector3 up = (std::abs(lightDir.y) < 0.99f) ? Vector3::UnitY : Vector3::UnitZ;

    return Matrix::CreateLookAt(eye, center, up);
}

//-----------------------------------------------------
// カスケード深度パスの開始
//-----------------------------------------------------
void ShadowMapRenderer::beginCascadePass(ID3D12GraphicsCommandList* cmd, int cascade)
{
    assert(cascade >= 0 && cascade < CascadeCount);

    // 深度書き込みステートへ遷移（まだ遷移済みでなければ）
    if (m_textureState != D3D12_RESOURCE_STATE_DEPTH_WRITE)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_shadowTexture.Get(),
            m_textureState,
            D3D12_RESOURCE_STATE_DEPTH_WRITE
        );
        cmd->ResourceBarrier(1, &barrier);
        m_textureState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    // DSV セット（カラー RT なし）
    cmd->OMSetRenderTargets(0, nullptr, FALSE, &m_dsvHandles[cascade]);

    // 深度バッファクリア
    cmd->ClearDepthStencilView(
        m_dsvHandles[cascade],
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f, 0,
        0, nullptr
    );

    // ビューポート & シザー設定
    D3D12_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<float>(ShadowMapSize);
    vp.Height = static_cast<float>(ShadowMapSize);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    cmd->RSSetViewports(1, &vp);

    D3D12_RECT scissor = { 0, 0, static_cast<LONG>(ShadowMapSize), static_cast<LONG>(ShadowMapSize) };
    cmd->RSSetScissorRects(1, &scissor);
}

//-----------------------------------------------------
// テクスチャステート遷移
//-----------------------------------------------------
void ShadowMapRenderer::transitionToSRV(ID3D12GraphicsCommandList* cmd)
{
    if (m_textureState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) return;

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_shadowTexture.Get(),
        m_textureState,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    cmd->ResourceBarrier(1, &barrier);
    m_textureState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void ShadowMapRenderer::transitionToDSV(ID3D12GraphicsCommandList* cmd)
{
    if (m_textureState == D3D12_RESOURCE_STATE_DEPTH_WRITE) return;

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_shadowTexture.Get(),
        m_textureState,
        D3D12_RESOURCE_STATE_DEPTH_WRITE
    );
    cmd->ResourceBarrier(1, &barrier);
    m_textureState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
}

//-----------------------------------------------------
// ゲッター
//-----------------------------------------------------
D3D12_GPU_VIRTUAL_ADDRESS ShadowMapRenderer::getLightVPCBAddress(int cascade) const
{
    return m_lightVPCBs[cascade]->getGPUAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS ShadowMapRenderer::getShadowParamsCBAddress() const
{
    return m_shadowParamsCB->getGPUAddress();
}

D3D12_GPU_DESCRIPTOR_HANDLE ShadowMapRenderer::getShadowMapSRVHandle() const
{
    return DescriptorHeapManager::Instance().getGPUHandle(m_srvIndex);
}

//-----------------------------------------------------
// ImGui デバッグ
//-----------------------------------------------------
void ShadowMapRenderer::debugImGui()
{
    if (!ImGui::CollapsingHeader("ShadowMapRenderer")) return;

    ImGui::SliderFloat("Shadow Bias", &m_shadowParams.shadowBias, 0.0f, 0.01f, "%.5f");
    ImGui::SliderFloat("Shadow Strength", &m_shadowParams.shadowStrength, 0.0f, 1.0f);
    ImGui::DragFloat3("Light Direction", &m_lightDir.x, 0.01f);

    m_shadowParams.shadowMapSize = static_cast<float>(ShadowMapSize);

    ImGui::SeparatorText("PCSS");
    ImGui::SliderFloat("Light Radius", &m_shadowParams.pcssLightRadius, 0.0f, 0.5f);
    ImGui::SliderFloat("Filter Radius Min", &m_shadowParams.pcssMinFilterRadius, 0.25f, 3.0f);
    ImGui::SliderFloat("Filter Radius Max", &m_shadowParams.pcssMaxFilterRadius, 1.0f, 12.0f);
    ImGui::SliderFloat("Blocker Search Radius", &m_shadowParams.pcssBlockerSearchRadius, 0.5f, 6.0f);
    ImGui::SliderFloat("Cascade Radius Scale", &m_shadowParams.pcssCascadeScale, 0.0f, 1.0f);

    // min/max が逆転しないよう補正
    m_shadowParams.pcssMaxFilterRadius = std::max(m_shadowParams.pcssMaxFilterRadius, m_shadowParams.pcssMinFilterRadius + 0.01f);

    ImGui::SeparatorText("Contact Shadow");
    ImGui::SliderFloat("Contact Length", &m_shadowParams.contactShadowLength, 0.0f, 2.5f);
    ImGui::SliderFloat("Contact Strength", &m_shadowParams.contactShadowStrength, 0.0f, 1.0f);
    ImGui::SliderFloat("Contact Depth Bias", &m_shadowParams.contactShadowDepthBias, 0.0f, 0.003f, "%.6f");
    ImGui::SliderFloat("Contact Normal Bias", &m_shadowParams.contactShadowNormalBias, 0.0f, 0.05f, "%.4f");
    int contactSteps = static_cast<int>(std::round(m_shadowParams.contactShadowStepCount));
    if (ImGui::SliderInt("Contact Steps", &contactSteps, 2, 10))
    {
        m_shadowParams.contactShadowStepCount = static_cast<float>(contactSteps);
    }

    ImGui::Text("Cascade Splits:");
    const float* splits = reinterpret_cast<const float*>(&m_shadowParams.cascadeSplits);
    for (int i = 0; i < CascadeCount; ++i)
        ImGui::Text("  [%d] %.2f", i, splits[i]);
}