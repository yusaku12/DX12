#include "pch.h"
#include "Camera/CameraManager.h"
#include "Graphics/IBLManager.h"
#include "SkyboxComponent.h"

SkyboxComponent::SkyboxComponent(const std::wstring& cubemapPath)
    : m_cubemapPath(cubemapPath)
{
    setName("Skybox");
}

void SkyboxComponent::awake()
{
    buildMesh();
    buildPSO();
    m_paramCB = DXMem::makeUnique<ConstantBuffer<SkyboxParams>>();

    if (!m_cubemapPath.empty())
    {
        setCubemap(m_cubemapPath);
    }
}

void SkyboxComponent::render(ID3D12GraphicsCommandList* cmd)
{
    renderForward(cmd);
}

void SkyboxComponent::renderGBuffer(ID3D12GraphicsCommandList* cmd)
{
    (void)cmd;
}

void SkyboxComponent::renderForward(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd || !isActiveInHierarchy()) return;
    if (!m_cubemap || !m_cubemap->isValid()) return;
    if (!m_vertexBuffer || !m_indexBuffer || !m_paramCB) return;

    draw(cmd);
}

void SkyboxComponent::inspectGUI()
{
    ImGui::Text(reinterpret_cast<const char*>(u8"Skybox"));

    if (ImGui::Button(reinterpret_cast<const char*>(u8"キューブマップ選択")))
    {
        std::vector<std::wstring> selectedFiles;
        DialogResult result = Dialog::openFile(
            selectedFiles,
            L"Select Cubemap",
            L"",
            false
        );

        if (result == DialogResult::OK && !selectedFiles.empty())
        {
            setCubemap(selectedFiles[0]);
        }
    }

    std::string pathText = m_cubemapPath.empty()
        ? "(None)"
        : wstringToString(m_cubemapPath);
    ImGui::Text("%s", pathText.c_str());

    ImGui::Separator();

    ImGui::SliderFloat(reinterpret_cast<const char*>(u8"露光"), &m_exposure, 0.0f, 8.0f);

    float tint[3] = { m_tint.x, m_tint.y, m_tint.z };
    if (ImGui::ColorEdit3(reinterpret_cast<const char*>(u8"色調"), tint, ImGuiColorEditFlags_Float))
    {
        m_tint = Vector3(tint[0], tint[1], tint[2]);
    }

    ImGui::SliderFloat(reinterpret_cast<const char*>(u8"回転(度)"), &m_rotationDegrees, -180.0f, 180.0f);
}

void SkyboxComponent::setCubemap(const std::wstring& path)
{
    m_cubemapPath = path;
    m_cubemap = TextureManager::Instance().load(path);
    IBLManager::Instance().setEnvironmentCubemap(path);
}

void SkyboxComponent::buildMesh()
{
    static const Vertex vertices[] =
    {
        { Vector3(-1, -1, -1) },
        { Vector3(-1,  1, -1) },
        { Vector3(1,  1, -1) },
        { Vector3(1, -1, -1) },
        { Vector3(-1, -1,  1) },
        { Vector3(-1,  1,  1) },
        { Vector3(1,  1,  1) },
        { Vector3(1, -1,  1) },
    };

    static const uint16_t indices[] =
    {
        0, 1, 2, 0, 2, 3, // -Z
        4, 6, 5, 4, 7, 6, // +Z
        4, 5, 1, 4, 1, 0, // -X
        3, 2, 6, 3, 6, 7, // +X
        1, 5, 6, 1, 6, 2, // +Y
        4, 0, 3, 4, 3, 7, // -Y
    };

    m_vertexBuffer = DXMem::makeUnique<VertexBuffer<Vertex>>(vertices);
    m_indexBuffer = DXMem::makeUnique<IndexBuffer<uint16_t>>(indices);
}

void SkyboxComponent::buildPSO()
{
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::Skybox;
    psoData.vsShaderId = ShaderID::SkyboxVS;
    psoData.psShaderId = ShaderID::SkyboxPS;
    psoData.rasterizerState = RasterizerState::CULL_NONE;
    psoData.blendState = BlendState::OPAQUE;
    psoData.depthStencilState = DepthStencilState::DEPTH_READ;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout =
    {
        D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    m_psoKey = PSOCreator::Instance().registerPSO(psoData);
}

void SkyboxComponent::updateParams()
{
    SkyboxParams params{};
    params.tint = m_tint;
    params.exposure = m_exposure;
    params.rotation = XMConvertToRadians(m_rotationDegrees);
    m_paramCB->update(params);
}

void SkyboxComponent::draw(ID3D12GraphicsCommandList* cmd)
{
    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    PSOCreator::Instance().setPSO(m_psoKey, cmd);

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetGraphicsRootConstantBufferView(0, CameraManager::Instance().getGPUAddress());

    updateParams();
    cmd->SetGraphicsRootConstantBufferView(1, m_paramCB->getGPUAddress());

    auto handle = m_cubemap->getGPUHandle();
    if (handle.ptr == 0) return;
    cmd->SetGraphicsRootDescriptorTable(2, handle);

    m_vertexBuffer->bind(cmd);
    m_indexBuffer->bind(cmd);
    cmd->DrawIndexedInstanced(m_indexBuffer->getIndexCount(), 1, 0, 0, 0);
}