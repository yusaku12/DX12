#include "pch.h"
#include "polygon.h"

Polygon::Polygon()
{
    //! 四角形
    Vertex vertices[] =
    {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
        {{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    };

    //! インデックス
    unsigned short indices[] =
    {
        0,1,2,
        2,1,3
    };

    auto dx12 = DX12::getInstance();

    //! 頂点バッファとインデックスバッファ
    D3D12_HEAP_PROPERTIES heapprop = {};
    heapprop.Type = D3D12_HEAP_TYPE_UPLOAD; //!< CPUからアクセスできる(今回はMapを使うのでできるように)
    heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resdesc = {};
    resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resdesc.Width = sizeof(vertices);    //!< テクスチャを使用する際はテクスチャの縦、横を入れる
    resdesc.Height = 1;
    resdesc.DepthOrArraySize = 1;
    resdesc.MipLevels = 1;
    resdesc.Format = DXGI_FORMAT_UNKNOWN;//!< テクスチャの場合は適したのを選択
    resdesc.SampleDesc.Count = 1;        //!< 頂点バッファなのでアンチエイリアシング関係ないので1
    resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; //!< D3D12_TEXTURE_LAYOUT_UNKNOWNを入れると自動的に適切なテクスチャが入るが今回がテクスチャではない

    //! GPUから読み取るので下記
    HRESULT hr = dx12.getDevice()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_vertexBuffer.GetAddressOf()));

    //! Mapを使用して頂点データをGPUに渡す(頂点データを頂点バッファに書き込む)
    unsigned char* vertMap = nullptr;
    hr = m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertMap));
    if (!SUCCEEDED(hr))return;
    memcpy(vertMap, vertices, sizeof(vertices));
    m_vertexBuffer->Unmap(0, nullptr);

    //! 頂点バッファビュー
    vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress(); //!< 頂点バッファ仮想アドレス
    vbView.SizeInBytes = sizeof(vertices);      //!< 全バイト数
    vbView.StrideInBytes = sizeof(vertices[0]); //!< 頂点１つのバイト数

    //! インデックスバッファー(こちらも同様にマップで読み取り)
    resdesc.Width = sizeof(indices);
    hr = dx12.getDevice()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_indexBuffer.GetAddressOf()));
    if (!SUCCEEDED(hr))return;

    unsigned short* indexMap = nullptr;
    m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indexMap));
    memcpy(indexMap, indices, sizeof(indices));
    m_indexBuffer->Unmap(0, nullptr);

    //! インデックスバッファビュー
    ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    ibView.Format = DXGI_FORMAT_R16_UINT;
    ibView.SizeInBytes = sizeof(indices);

    //! 頂点レイアウト
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        {
            "POSITION",                                     //!< シェーダの方で記載したものに合わせる
            0,                                              //!< 同じ名前で定義した時は複数対応できるようになっている
            DXGI_FORMAT_R32G32B32_FLOAT,                    //!< フォーマットメモリ数に合わせて設定
            0,                                              //!< スロット番号複数の頂点データを合わせて使用する場合
            D3D12_APPEND_ALIGNED_ELEMENT,                   //!< データオフセットを自動で計算してくれるのでこれしかありえない
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,     //!< 1つの頂点ごとに1つのレイアウトが含まれる
            0                                               //!< インスタンシングを使用する際に複数のデータが同じデータを再利用するためのもの
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

    //! テクスチャ読み込み
    tex = TextureManager::Instance().load(L"Data/Texture/barria.png");

    //! シェーダー読み込み
    ShaderManager::Instance().load(L"HLSL\\polygonVS.hlsl", ShaderType::VS, gpipeline);
    ShaderManager::Instance().load(L"HLSL\\polygonPS.hlsl", ShaderType::PS, gpipeline);

    //! サンプリングとリソース設定
    tex->setRootSignature(m_rootSignature);

    //! ルートシグネチャ設定
    gpipeline.pRootSignature = m_rootSignature.Get();

    //! パイプライン設定
    setPlpelineStateObject(&gpipeline, BlendState::ALPHA, DepthStencilState::DEPTH_NONE, RasterizerState::CULL_NONE);

    //! レンダーターゲットのブレンド無効
    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
    renderTargetBlendDesc.BlendEnable = false;
    renderTargetBlendDesc.LogicOpEnable = false;
    renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

    //! 入力レイアウトを設定
    gpipeline.InputLayout.pInputElementDescs = inputLayout;
    gpipeline.InputLayout.NumElements = _countof(inputLayout);
    gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    //! 出力先
    gpipeline.NumRenderTargets = 1;
    gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    gpipeline.DSVFormat = DXGI_FORMAT_UNKNOWN; // 深度バッファなし
    gpipeline.SampleDesc.Count = 1;
    gpipeline.SampleDesc.Quality = 0;
    gpipeline.SampleMask = UINT_MAX;                   //! 深度バッファ使わない場合でも指定は必須（空なら UNKNOWN）
    gpipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    //! パイプラインステート生成
    hr = dx12.getDevice()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(m_pipelineState.GetAddressOf()));
    if (FAILED(hr))
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "Failed to create pipeline state");
    }
}

Polygon::~Polygon()
{
}

void Polygon::render()
{
    auto dx12 = DX12::getInstance();

    dx12.getGraphicsCommandList()->SetPipelineState(m_pipelineState.Get());
    dx12.getGraphicsCommandList()->SetGraphicsRootSignature(m_rootSignature.Get());

    dx12.getGraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dx12.getGraphicsCommandList()->IASetVertexBuffers(0, 1, &vbView);
    dx12.getGraphicsCommandList()->IASetIndexBuffer(&ibView);

    tex->applyTexture();

    dx12.getGraphicsCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}