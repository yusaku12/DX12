#include "pch.h"

void DebugPrimitive::initialize(UINT maxLines)
{
    m_maxLines = maxLines;
    m_lines.reserve(m_maxLines);

    //! PSO 作成 (線描画用)
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
        D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        D3D12_INPUT_ELEMENT_DESC{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_psoCreator = std::make_unique<PSOCreator>(pso);

    ensureBuffer();
}

void DebugPrimitive::shutdown()
{
    m_uploadBuffer.reset();
    m_psoCreator.reset();
    m_lines.clear();
    m_mappedVertices = nullptr;
}

void DebugPrimitive::ensureBuffer()
{
    //! 頂点数 = ライン数 * 2
    UINT maxVertices = (m_maxLines == 0) ? 65536 * 2u : m_maxLines * 2u;
    m_vertexBufferSize = sizeof(Vertex) * maxVertices;

    m_uploadBuffer = std::make_unique<UploadBuffer>(m_vertexBufferSize);
    auto res = m_uploadBuffer->getResource();
    HRESULT hr = res->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedVertices));
    LOG_HR(hr, "DebugPrimitive: failed to Map upload buffer");

    m_vbv.BufferLocation = res->GetGPUVirtualAddress();
    m_vbv.StrideInBytes = sizeof(Vertex);
    m_vbv.SizeInBytes = m_vertexBufferSize;
}

void DebugPrimitive::beginFrame()
{
    //! フレーム開始時は特に何もしない（必要ならここで一時データのクリアを行う）
}

void DebugPrimitive::update(float deltaTime)
{
    //! 寿命付きラインの減算・削除
    if (m_lines.empty()) return;

    for (size_t i = 0; i < m_lines.size(); )
    {
        if (m_lines[i].remaining > 0.0f)
        {
            m_lines[i].remaining -= deltaTime;
            if (m_lines[i].remaining <= 0.0f)
            {
                // erase
                m_lines.erase(m_lines.begin() + static_cast<int>(i));
                continue;
            }
        }
        ++i;
    }
}

void DebugPrimitive::buildVertexBuffer()
{
    if (m_lines.empty()) return;

    //! 実際に書き込む頂点数
    UINT vertexCount = static_cast<UINT>(m_lines.size()) * 2u;

    //! 上限チェック
    UINT maxVertices = m_vbv.SizeInBytes / m_vbv.StrideInBytes;
    if (vertexCount > maxVertices)
    {
        //! 超過時は描画できる分だけ描く（先頭の方を優先）
        vertexCount = maxVertices;
    }

    //! マップ済みメモリに書き込み（UploadBuffer は連続領域）
    Vertex* dst = m_mappedVertices;
    UINT idx = 0;
    for (const auto& L : m_lines)
    {
        if (idx + 1 >= vertexCount) break;
        dst[idx++] = Vertex{ L.a, L.color };
        dst[idx++] = Vertex{ L.b, L.color };
    }

    //! VBView のサイズを実際の頂点数に合わせる
    m_vbv.SizeInBytes = static_cast<UINT>(vertexCount * sizeof(Vertex));
}

void DebugPrimitive::render()
{
    if (m_lines.empty()) return;

    //! DescriptorHeap をセット（既存のコードに合わせる）
    DescriptorHeapManager::Instance().setDiscriptorHeap();

    auto cmd = DX12::Instance().getGraphicsCommandList();

    //! ルートシグネチャ & PSO
    m_psoCreator->setPSO();

    //! Camera CBV をセット（既存プロジェクトの慣習に合わせる）
    cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    //! ビルド頂点バッファ
    buildVertexBuffer();

    //! VB バインド
    cmd->IASetVertexBuffers(0, 1, &m_vbv);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    //! Draw
    UINT vertexCount = m_vbv.SizeInBytes / m_vbv.StrideInBytes;
    if (vertexCount > 0)
    {
        cmd->DrawInstanced(vertexCount, 1, 0, 0);
    }
}

void DebugPrimitive::addLine(const Vector3& a, const Vector3& b, const Vector4& color, float duration)
{
    if (m_maxLines > 0 && m_lines.size() >= m_maxLines)
    {
        //! 上限到達時は最古を上書き（リング不要なら削る）
        m_lines.erase(m_lines.begin());
    }
    Line l;
    l.a = a;
    l.b = b;
    l.color = color;
    l.remaining = duration;
    m_lines.push_back(l);
}

void DebugPrimitive::addBox(const Vector3& center, const Vector3& extents, const Vector4& color, float duration)
{
    //! 8頂点から 12 本の線を追加
    Vector3 v[8];
    Vector3 e = extents;
    v[0] = center + Vector3(-e.x, -e.y, -e.z);
    v[1] = center + Vector3(e.x, -e.y, -e.z);
    v[2] = center + Vector3(e.x, e.y, -e.z);
    v[3] = center + Vector3(-e.x, e.y, -e.z);
    v[4] = center + Vector3(-e.x, -e.y, e.z);
    v[5] = center + Vector3(e.x, -e.y, e.z);
    v[6] = center + Vector3(e.x, e.y, e.z);
    v[7] = center + Vector3(-e.x, e.y, e.z);

    const int edges[12][2] =
    {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (int i = 0; i < 12; ++i)
    {
        addLine(v[edges[i][0]], v[edges[i][1]], color, duration);
    }
}

void DebugPrimitive::addTransform(const Matrix& transform, float size)
{
    Vector3 origin = transform.Translation();
    Vector3 x = transform.Right() * size;
    Vector3 y = transform.Up() * size;
    Vector3 z = transform.Forward() * size;

    addLine(origin, origin + x, Vector4(1, 0, 0, 1));
    addLine(origin, origin + y, Vector4(0, 1, 0, 1));
    addLine(origin, origin + z, Vector4(0, 0, 1, 1));
}

void DebugPrimitive::addSphere(const Vector3& center, float radius, int segments, int rings, const Vector4& color, float duration)
{
    if (radius <= 0.0f) return;
    if (segments < 3) segments = 3;
    if (rings < 2) rings = 2;

    const float PI = 3.14159265358979323846f;
    //! 緯線（latitude）の円（Y方向で高さを変えつつ XZ 平面に円を描く）
    for (int r = 1; r < rings; ++r)
    {
        float theta = PI * static_cast<float>(r) / static_cast<float>(rings); //!< (0, PI)
        float y = std::cos(theta) * radius;
        float ringR = std::sin(theta) * radius;

        //! 円を構成する線分
        Vector3 prevPoint;
        for (int s = 0; s <= segments; ++s)
        {
            float phi = 2.0f * PI * static_cast<float>(s) / static_cast<float>(segments);
            float x = std::cos(phi) * ringR;
            float z = std::sin(phi) * ringR;
            Vector3 p = center + Vector3(x, y, z);
            if (s > 0) addLine(prevPoint, p, color, duration);
            prevPoint = p;
        }
    }

    //! 子午線（longitude）: 各経度方向に沿って緯度点を繋ぐ
    for (int s = 0; s < segments; ++s)
    {
        float phi = 2.0f * PI * static_cast<float>(s) / static_cast<float>(segments);
        Vector3 prevPoint = center + Vector3(0.0f, radius, 0.0f); //!< 北極開始
        //! 緯度に沿って下へ
        for (int r = 1; r <= rings; ++r)
        {
            float theta = PI * static_cast<float>(r) / static_cast<float>(rings); // 0..PI
            float y = std::cos(theta) * radius;
            float ringR = std::sin(theta) * radius;
            float x = std::cos(phi) * ringR;
            float z = std::sin(phi) * ringR;
            Vector3 p = center + Vector3(x, y, z);
            addLine(prevPoint, p, color, duration);
            prevPoint = p;
        }
    }
}

void DebugPrimitive::addGrid(const Vector3& center, float width, float depth, float step, const Vector4& color, float duration)
{
    if (width <= 0.0f || depth <= 0.0f) return;
    if (step <= 0.0f) step = 1.0f;

    float halfW = width * 0.5f;
    float halfD = depth * 0.5f;

    //! X方向の等間隔線（Z軸に沿う線）
    int linesX = static_cast<int>(std::floor(width / step)) + 1;
    //! Z方向の等間隔線（X軸に沿う線）
    int linesZ = static_cast<int>(std::floor(depth / step)) + 1;

    //! 中心を基準に -half..+half を走査
    float startX = center.x - halfW;
    float startZ = center.z - halfD;

    for (int i = 0; i < linesX; ++i)
    {
        float x = startX + i * step;
        Vector3 a = Vector3(x, center.y, center.z - halfD);
        Vector3 b = Vector3(x, center.y, center.z + halfD);
        addLine(a, b, color, duration);
    }

    for (int j = 0; j < linesZ; ++j)
    {
        float z = startZ + j * step;
        Vector3 a = Vector3(center.x - halfW, center.y, z);
        Vector3 b = Vector3(center.x + halfW, center.y, z);
        addLine(a, b, color, duration);
    }

    //! 中心線（X,Z軸）を目立たせる（オプション）：中心に赤/青
    Vector3 cx1 = Vector3(center.x - halfW, center.y, center.z);
    Vector3 cx2 = Vector3(center.x + halfW, center.y, center.z);
    addLine(cx1, cx2, Vector4(0.75f, 0.75f, 0.75f, 1.0f), duration);

    Vector3 cz1 = Vector3(center.x, center.y, center.z - halfD);
    Vector3 cz2 = Vector3(center.x, center.y, center.z + halfD);
    addLine(cz1, cz2, Vector4(0.75f, 0.75f, 0.75f, 1.0f), duration);
}

void DebugPrimitive::clear()
{
    m_lines.clear();
}