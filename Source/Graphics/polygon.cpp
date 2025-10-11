#include "pch.h"
#include "polygon.h"

Polygon::Polygon()
{
    //! �l�p�`
    Vertex vertices[] =
    {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
        {{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    };

    //! �C���f�b�N�X
    unsigned short indices[] =
    {
        0,1,2,
        2,1,3
    };

    auto dx12 = DX12::getInstance();

    //! ���_�o�b�t�@�ƃC���f�b�N�X�o�b�t�@
    D3D12_HEAP_PROPERTIES heapprop = {};
    heapprop.Type = D3D12_HEAP_TYPE_UPLOAD; //!< CPU����A�N�Z�X�ł���(�����Map���g���̂łł���悤��)
    heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resdesc = {};
    resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resdesc.Width = sizeof(vertices);    //!< �e�N�X�`�����g�p����ۂ̓e�N�X�`���̏c�A��������
    resdesc.Height = 1;
    resdesc.DepthOrArraySize = 1;
    resdesc.MipLevels = 1;
    resdesc.Format = DXGI_FORMAT_UNKNOWN;//!< �e�N�X�`���̏ꍇ�͓K�����̂�I��
    resdesc.SampleDesc.Count = 1;        //!< ���_�o�b�t�@�Ȃ̂ŃA���`�G�C���A�V���O�֌W�Ȃ��̂�1
    resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; //!< D3D12_TEXTURE_LAYOUT_UNKNOWN������Ǝ����I�ɓK�؂ȃe�N�X�`�������邪���񂪃e�N�X�`���ł͂Ȃ�

    //! GPU����ǂݎ��̂ŉ��L
    HRESULT hr = dx12.getDevice()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_vertexBuffer.GetAddressOf()));

    //! Map���g�p���Ē��_�f�[�^��GPU�ɓn��(���_�f�[�^�𒸓_�o�b�t�@�ɏ�������)
    unsigned char* vertMap = nullptr;
    hr = m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertMap));
    if (!SUCCEEDED(hr))return;
    memcpy(vertMap, vertices, sizeof(vertices));
    m_vertexBuffer->Unmap(0, nullptr);

    //! ���_�o�b�t�@�r���[
    vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress(); //!< ���_�o�b�t�@���z�A�h���X
    vbView.SizeInBytes = sizeof(vertices);      //!< �S�o�C�g��
    vbView.StrideInBytes = sizeof(vertices[0]); //!< ���_�P�̃o�C�g��

    //! �C���f�b�N�X�o�b�t�@�[(����������l�Ƀ}�b�v�œǂݎ��)
    resdesc.Width = sizeof(indices);
    hr = dx12.getDevice()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_indexBuffer.GetAddressOf()));
    if (!SUCCEEDED(hr))return;

    unsigned short* indexMap = nullptr;
    m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indexMap));
    memcpy(indexMap, indices, sizeof(indices));
    m_indexBuffer->Unmap(0, nullptr);

    //! �C���f�b�N�X�o�b�t�@�r���[
    ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    ibView.Format = DXGI_FORMAT_R16_UINT;
    ibView.SizeInBytes = sizeof(indices);

    //! �e�퐶��
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;

    //! �V�F�[�_�[�ǂݍ���
    LoadShader::getInstance().loadShader(L"HLSL\\polygonVS.fx", ShaderType::VS, vsBlob, errorBlob);
    LoadShader::getInstance().loadShader(L"HLSL\\polygonPS.fx", ShaderType::PS, psBlob, errorBlob);

    //! ���_���C�A�E�g
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        {
            "POSITION",                                     //!< �V�F�[�_�̕��ŋL�ڂ������̂ɍ��킹��
            0,                                              //!< �������O�Œ�`�������͕����Ή��ł���悤�ɂȂ��Ă���
            DXGI_FORMAT_R32G32B32_FLOAT,                    //!< �t�H�[�}�b�g���������ɍ��킹�Đݒ�
            0,                                              //!< �X���b�g�ԍ������̒��_�f�[�^�����킹�Ďg�p����ꍇ
            D3D12_APPEND_ALIGNED_ELEMENT,                   //!< �f�[�^�I�t�Z�b�g�������Ōv�Z���Ă����̂ł��ꂵ�����肦�Ȃ�
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,     //!< 1�̒��_���Ƃ�1�̃��C�A�E�g���܂܂��
            0                                               //!< �C���X�^���V���O���g�p����ۂɕ����̃f�[�^�������f�[�^���ė��p���邽�߂̂���
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
    };

    //! ���[�g�V�O�l�`��(�ǂ̃V�F�[�_���\�[�X���g�p����̂��A�ǂ̃T���v���[���g�p����̂���ݒ肷�����)
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 0;
    rootSignatureDesc.pParameters = nullptr;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;  //!< ���_��񂪑��݂���
    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, m_rootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());
    hr = dx12.getDevice()->CreateRootSignature(0, m_rootSigBlob->GetBufferPointer(), m_rootSigBlob->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.GetAddressOf()));
    m_rootSigBlob->Release();

    //! �g���V�F�[�_��ݒ�
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
    gpipeline.pRootSignature = m_rootSignature.Get();
    gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
    gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
    gpipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
    gpipeline.PS.BytecodeLength = psBlob->GetBufferSize();

    //! ��{�I�ȃT���v�����O�}�X�N���g�p���A�A���`�G�C���A�V���O�͎g�p���Ȃ��悤��
    //! �J�����O�͂����A�����͖��߂�悤�ɐݒ�
    //! �[�x�ɂ��N���b�s���O�͗L���ɂ���
    gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    gpipeline.RasterizerState.FrontCounterClockwise = FALSE;
    gpipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    gpipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    gpipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    gpipeline.RasterizerState.DepthClipEnable = TRUE;
    gpipeline.RasterizerState.MultisampleEnable = FALSE;
    gpipeline.RasterizerState.AntialiasedLineEnable = FALSE;
    gpipeline.RasterizerState.ForcedSampleCount = 0;
    gpipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    //! �A���t�@�e�X�g�ƃA���t�@�u�����h����
    gpipeline.BlendState.AlphaToCoverageEnable = false;
    gpipeline.BlendState.IndependentBlendEnable = false;

    //! �����_�[�^�[�Q�b�g�̃u�����h����
    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
    renderTargetBlendDesc.BlendEnable = false;
    renderTargetBlendDesc.LogicOpEnable = false;
    renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

    gpipeline.DepthStencilState.DepthEnable = FALSE;
    gpipeline.DepthStencilState.StencilEnable = FALSE;

    //! ���̓��C�A�E�g��ݒ�
    gpipeline.InputLayout.pInputElementDescs = inputLayout;
    gpipeline.InputLayout.NumElements = _countof(inputLayout);

    gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    //! �o�͐�
    gpipeline.NumRenderTargets = 1;
    gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    gpipeline.DSVFormat = DXGI_FORMAT_UNKNOWN; // �[�x�o�b�t�@�Ȃ�
    gpipeline.SampleDesc.Count = 1;
    gpipeline.SampleDesc.Quality = 0;

    //! �[�x�o�b�t�@�g��Ȃ��ꍇ�ł��w��͕K�{�i��Ȃ� UNKNOWN�j
    gpipeline.SampleMask = UINT_MAX;
    gpipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    //! �p�C�v���C���X�e�[�g����
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

    dx12.getGraphicsCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}