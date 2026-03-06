#include "pch.h"

static constexpr float PI = 3.14159265358979323846f;
static constexpr float PI2 = PI * 2.0f;
static constexpr float PI_H = PI * 0.5f;

void DebugPrimitive::initialize()
{
    //! メッシュ作成
    createSphereMesh(16);
    createHalfSphereMesh(16);
    createCylinderMesh(16);
    createBoxMesh();
    createLineMesh();

    //! メッシュ定数バッファ作成（ドローコールごとに個別エレメント）
    m_meshCB = std::make_unique<ConstantBuffer<CbMesh>>(MAX_DRAW_CALLS);

    //! グリッド用動的バッファ（最大 2048 ライン = 4096 頂点）
    constexpr UINT gridMaxVerts = 4096;
    m_gridUploadBuffer = std::make_unique<UploadBuffer>(sizeof(Vertex) * gridMaxVerts);
    auto* res = m_gridUploadBuffer->getResource();
    res->Map(0, nullptr, reinterpret_cast<void**>(&m_gridMapped));
    m_gridVBV.BufferLocation = res->GetGPUVirtualAddress();
    m_gridVBV.StrideInBytes = sizeof(Vertex);
    m_gridVBV.SizeInBytes = sizeof(Vertex) * gridMaxVerts;

    //! PSO 作成（メッシュ描画用：位置 + カラー）
    PSOCreator::PSOData pso;
    pso.rootSignatureType = RootSignatureType::DebugPrimitive;
    pso.vsShaderId = ShaderID::DebugPrimitiveVS;
    pso.psShaderId = ShaderID::DebugPrimitivePS;
    pso.rasterizerState = RasterizerState::CULL_NONE;
    pso.blendState = BlendState::OPAQUE;
    pso.depthStencilState = DepthStencilState::DEPTH_DEFALT;
    pso.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    pso.inputLayout =
    {
        D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_meshPsoKey = PSOCreator::Instance().registerPSO(pso);

    //! リクエスト予約
    m_sphereRequests.reserve(64);
    m_halfSphereRequests.reserve(128);
    m_boxRequests.reserve(64);
    m_cylinderRequests.reserve(64);
    m_lineRequests.reserve(2048);
}

void DebugPrimitive::shutdown()
{
    m_meshCB.reset();
    m_gridUploadBuffer.reset();
    m_sphereVB.Reset();
    m_halfSphereVB.Reset();
    m_cylinderVB.Reset();
    m_boxVB.Reset();
    m_lineVB.Reset();
}

void DebugPrimitive::beginFrame()
{
    m_sphereRequests.clear();
    m_halfSphereRequests.clear();
    m_boxRequests.clear();
    m_cylinderRequests.clear();
    m_lineRequests.clear();
    m_drawIndex = 0;
}

void DebugPrimitive::drawSphere(const Matrix& world, float radius, const Vector4& color)
{
    m_sphereRequests.push_back({ world, radius, color });
}

void DebugPrimitive::drawBox(const Matrix& world, const Vector3& extents, const Vector4& color)
{
    m_boxRequests.push_back({ world, extents, color });
}

void DebugPrimitive::drawCylinder(const Matrix& world, float radius, float height, const Vector4& color)
{
    m_cylinderRequests.push_back({ world, radius, height, color });
}

void DebugPrimitive::drawCapsule(const Matrix& world, float radius, float halfHeight, const Vector4& color)
{
    //! ワールド行列から平行移動を分離
    Vector3 worldPos = world.Translation();

    //! 回転部分のみの行列を作成（平行移動を除去）
    Matrix rs = world;
    rs._41 = 0.0f;
    rs._42 = 0.0f;
    rs._43 = 0.0f;

    //! 上半球（Y-方向に凸 → X軸でPI回転して蓋にする）
    {
        Matrix rot = Matrix::CreateRotationX(PI);
        Matrix w = rot * rs;
        Vector3 position = Vector3::Transform(Vector3(0, -halfHeight, 0), world);
        w.Translation(position);
        m_halfSphereRequests.push_back({ w, radius, color });
    }
    //! 円柱
    m_cylinderRequests.push_back({ world, radius, halfHeight * 2.0f, color });
    //! 下半球（Y+方向に凸、そのまま）
    {
        Matrix w = rs;
        Vector3 position = Vector3::Transform(Vector3(0, halfHeight, 0), world);
        w.Translation(position);
        m_halfSphereRequests.push_back({ w, radius, color });
    }
}

void DebugPrimitive::drawGrid(const Vector3& center, float width, float depth, float step, const Vector4& color)
{
    if (width <= 0.0f || depth <= 0.0f) return;
    if (step <= 0.0f) step = 1.0f;

    float halfW = width * 0.5f;
    float halfD = depth * 0.5f;

    int linesX = static_cast<int>(std::floor(width / step)) + 1;
    int linesZ = static_cast<int>(std::floor(depth / step)) + 1;

    float startX = center.x - halfW;
    float startZ = center.z - halfD;

    for (int i = 0; i < linesX; ++i)
    {
        float x = startX + i * step;
        m_lineRequests.push_back({ Vector3(x, center.y, center.z - halfD), Vector3(x, center.y, center.z + halfD), color });
    }
    for (int j = 0; j < linesZ; ++j)
    {
        float z = startZ + j * step;
        m_lineRequests.push_back({ Vector3(center.x - halfW, center.y, z), Vector3(center.x + halfW, center.y, z), color });
    }
}

void DebugPrimitive::render()
{
    bool hasAnything = !m_sphereRequests.empty()
        || !m_halfSphereRequests.empty()
        || !m_boxRequests.empty()
        || !m_cylinderRequests.empty()
        || !m_lineRequests.empty();
    if (!hasAnything) return;

    DescriptorHeapManager::Instance().setDescriptorHeap();
    PSOCreator::Instance().setPSO(m_meshPsoKey);

    auto cmd = DX12::Instance().getGraphicsCommandList();
    cmd->SetGraphicsRootConstantBufferView(0, CameraManager::Instance().getGPUAddress());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    renderSpheres();
    renderBoxes();
    renderCylinders();
    renderCapsules();
    renderGrid();
}

void DebugPrimitive::renderSpheres()
{
    for (const auto& req : m_sphereRequests)
    {
        Matrix s = Matrix::CreateScale(req.radius);
        CbMesh cb;
        cb.world = (s * req.world).Transpose();
        cb.color = req.color;
        drawMesh(m_sphereVBV, m_sphereVertexCount, cb);
    }
}

void DebugPrimitive::renderBoxes()
{
    for (const auto& req : m_boxRequests)
    {
        Matrix s = Matrix::CreateScale(req.extents);
        CbMesh cb;
        cb.world = (s * req.world).Transpose();
        cb.color = req.color;
        drawMesh(m_boxVBV, m_boxVertexCount, cb);
    }
}

void DebugPrimitive::renderCylinders()
{
    for (const auto& req : m_cylinderRequests)
    {
        Matrix s = Matrix::CreateScale(req.radius, req.height, req.radius);
        CbMesh cb;
        cb.world = (s * req.world).Transpose();
        cb.color = req.color;
        drawMesh(m_cylinderVBV, m_cylinderVertexCount, cb);
    }
}

void DebugPrimitive::renderCapsules()
{
    for (const auto& req : m_halfSphereRequests)
    {
        Matrix s = Matrix::CreateScale(req.radius);
        CbMesh cb;
        cb.world = (s * req.world).Transpose();
        cb.color = req.color;
        drawMesh(m_halfSphereVBV, m_halfSphereVertexCount, cb);
    }
}

void DebugPrimitive::renderGrid()
{
    if (m_lineRequests.empty()) return;
    if (m_drawIndex >= MAX_DRAW_CALLS) return;

    //! グリッドラインを動的バッファに書き込み
    UINT maxVerts = m_gridVBV.SizeInBytes / m_gridVBV.StrideInBytes;
    UINT vertCount = std::min(static_cast<UINT>(m_lineRequests.size()) * 2u, maxVerts);

    UINT idx = 0;
    for (const auto& L : m_lineRequests)
    {
        if (idx + 1 >= vertCount) break;
        m_gridMapped[idx++] = { L.a, L.color };
        m_gridMapped[idx++] = { L.b, L.color };
    }

    UINT cbIdx = m_drawIndex++;
    CbMesh cb;
    cb.world = Matrix::Identity;
    cb.color = Vector4::One;
    m_meshCB->update(cb, cbIdx);

    auto cmd = DX12::Instance().getGraphicsCommandList();
    cmd->SetGraphicsRootConstantBufferView(1, m_meshCB->getGPUAddress(cbIdx));

    D3D12_VERTEX_BUFFER_VIEW vbv = m_gridVBV;
    vbv.SizeInBytes = idx * sizeof(Vertex);
    cmd->IASetVertexBuffers(0, 1, &vbv);
    cmd->DrawInstanced(idx, 1, 0, 0);
}

void DebugPrimitive::drawMesh(const D3D12_VERTEX_BUFFER_VIEW& vbv, UINT vertexCount, const CbMesh& cb)
{
    if (m_drawIndex >= MAX_DRAW_CALLS) return;

    UINT idx = m_drawIndex++;
    m_meshCB->update(cb, idx);

    auto cmd = DX12::Instance().getGraphicsCommandList();
    cmd->SetGraphicsRootConstantBufferView(1, m_meshCB->getGPUAddress(idx));
    cmd->IASetVertexBuffers(0, 1, &vbv);
    cmd->DrawInstanced(vertexCount, 1, 0, 0);
}

void DebugPrimitive::createVertexBuffer(const Vertex* vertices, UINT count,
    Microsoft::WRL::ComPtr<ID3D12Resource>& outBuffer,
    D3D12_VERTEX_BUFFER_VIEW& outVBV)
{
    auto device = DX12::Instance().getDevice();
    UINT bufferSize = sizeof(Vertex) * count;

    //! デフォルトヒープに配置
    auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    HRESULT hr = device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&outBuffer));
    LOG_HR(hr, "DebugPrimitive: createVertexBuffer failed");

    //! データ書き込み
    void* mapped = nullptr;
    outBuffer->Map(0, nullptr, &mapped);
    memcpy(mapped, vertices, bufferSize);
    outBuffer->Unmap(0, nullptr);

    //! VBV
    outVBV.BufferLocation = outBuffer->GetGPUVirtualAddress();
    outVBV.StrideInBytes = sizeof(Vertex);
    outVBV.SizeInBytes = bufferSize;
}

void DebugPrimitive::createSphereMesh(int subdivisions)
{
    static constexpr Vector4 defaultColor = { 1, 1, 1, 1 };
    m_sphereVertexCount = subdivisions * 2 * 3;
    auto verts = std::make_unique<Vertex[]>(m_sphereVertexCount);
    Vertex* p = verts.get();
    float step = PI2 / subdivisions;

    // XZ平面
    for (int i = 0; i < subdivisions; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            float t = step * ((i + j) % subdivisions);
            *p++ = { Vector3(sinf(t), 0.0f, cosf(t)), defaultColor };
        }
    }
    // XY平面
    for (int i = 0; i < subdivisions; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            float t = step * ((i + j) % subdivisions);
            *p++ = { Vector3(sinf(t), cosf(t), 0.0f), defaultColor };
        }
    }
    // YZ平面
    for (int i = 0; i < subdivisions; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            float t = step * ((i + j) % subdivisions);
            *p++ = { Vector3(0.0f, sinf(t), cosf(t)), defaultColor };
        }
    }

    createVertexBuffer(verts.get(), m_sphereVertexCount, m_sphereVB, m_sphereVBV);
}

void DebugPrimitive::createHalfSphereMesh(int subdivisions)
{
    static constexpr Vector4 defaultColor = { 1, 1, 1, 1 };
    int halfSub = subdivisions / 2;
    m_halfSphereVertexCount = subdivisions * 2 + halfSub * 2 + halfSub * 2;
    auto verts = std::make_unique<Vertex[]>(m_halfSphereVertexCount);
    Vertex* p = verts.get();
    float step = PI2 / subdivisions;

    // XZ平面（全周）
    for (int i = 0; i < subdivisions; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            float t = step * ((i + j) % subdivisions);
            *p++ = { Vector3(sinf(t), 0.0f, cosf(t)), defaultColor };
        }
    }
    // XY平面（半分）
    for (int i = 0; i < halfSub; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            float t = step * ((i + j) % subdivisions) - PI_H;
            *p++ = { Vector3(sinf(t), cosf(t), 0.0f), defaultColor };
        }
    }
    // YZ平面（半分）
    for (int i = 0; i < halfSub; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            float t = step * ((i + j) % subdivisions);
            *p++ = { Vector3(0.0f, sinf(t), cosf(t)), defaultColor };
        }
    }

    createVertexBuffer(verts.get(), m_halfSphereVertexCount, m_halfSphereVB, m_halfSphereVBV);
}

void DebugPrimitive::createCylinderMesh(int subdivisions)
{
    static constexpr Vector4 defaultColor = { 1, 1, 1, 1 };
    m_cylinderVertexCount = (subdivisions * 2 * 2) + (2 * 2 * 2);
    auto verts = std::make_unique<Vertex[]>(m_cylinderVertexCount);
    Vertex* p = verts.get();
    float step = PI2 / subdivisions;

    // 上リング
    for (int i = 0; i < subdivisions; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            float t = step * ((i + j) % subdivisions);
            *p++ = { Vector3(sinf(t), -0.5f, cosf(t)), defaultColor };
        }
    }
    // 下リング
    for (int i = 0; i < subdivisions; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            float t = step * ((i + j) % subdivisions);
            *p++ = { Vector3(sinf(t), 0.5f, cosf(t)), defaultColor };
        }
    }
    // 縦線 4本
    *p++ = { Vector3(0, -0.5f, 1), defaultColor };   *p++ = { Vector3(0, 0.5f, 1), defaultColor };
    *p++ = { Vector3(0, -0.5f, -1), defaultColor };  *p++ = { Vector3(0, 0.5f, -1), defaultColor };
    *p++ = { Vector3(1, -0.5f, 0), defaultColor };   *p++ = { Vector3(1, 0.5f, 0), defaultColor };
    *p++ = { Vector3(-1, -0.5f, 0), defaultColor };  *p++ = { Vector3(-1, 0.5f, 0), defaultColor };

    createVertexBuffer(verts.get(), m_cylinderVertexCount, m_cylinderVB, m_cylinderVBV);
}

void DebugPrimitive::createBoxMesh()
{
    static constexpr Vector4 defaultColor = { 1, 1, 1, 1 };
    //! 単位サイズ（-1 ～ +1）の箱
    Vector3 positions[8] =
    {
        { -1,  1, -1 }, {  1,  1, -1 }, {  1,  1,  1 }, { -1,  1,  1 },
        { -1, -1, -1 }, {  1, -1, -1 }, {  1, -1,  1 }, { -1, -1,  1 },
    };

    m_boxVertexCount = 24;
    Vertex verts[24] =
    {
        // top
        { positions[0], defaultColor }, { positions[1], defaultColor },
        { positions[1], defaultColor }, { positions[2], defaultColor },
        { positions[2], defaultColor }, { positions[3], defaultColor },
        { positions[3], defaultColor }, { positions[0], defaultColor },
        // bottom
        { positions[4], defaultColor }, { positions[5], defaultColor },
        { positions[5], defaultColor }, { positions[6], defaultColor },
        { positions[6], defaultColor }, { positions[7], defaultColor },
        { positions[7], defaultColor }, { positions[4], defaultColor },
        // pillars
        { positions[0], defaultColor }, { positions[4], defaultColor },
        { positions[1], defaultColor }, { positions[5], defaultColor },
        { positions[2], defaultColor }, { positions[6], defaultColor },
        { positions[3], defaultColor }, { positions[7], defaultColor },
    };

    createVertexBuffer(verts, m_boxVertexCount, m_boxVB, m_boxVBV);
}

void DebugPrimitive::createLineMesh()
{
    static constexpr Vector4 defaultColor = { 1, 1, 1, 1 };
    //! 単位ライン（0→1）
    Vertex verts[2] = { { Vector3::Zero, defaultColor }, { Vector3(1, 0, 0), defaultColor } };
    createVertexBuffer(verts, 2, m_lineVB, m_lineVBV);
}