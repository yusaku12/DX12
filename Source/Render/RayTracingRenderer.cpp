#include "pch.h"
#include "Render/RayTracingRenderer.h"
#include "Render/RenderPassBase.h"
#include "GameObject/GameObjectRegistry.h"
#include "Component/FbxRenderComponent.h"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <array>
#include <dxcapi.h>

namespace
{
    constexpr UINT64 kShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    constexpr UINT64 kShaderRecordAlignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
    constexpr UINT64 kShaderTableAlignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

    UINT64 alignTo(UINT64 value, UINT64 alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    std::wstring getExeDirectory()
    {
        wchar_t path[MAX_PATH] = {};
        const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (len == 0 || len >= MAX_PATH)
        {
            return L".";
        }

        std::filesystem::path p(path);
        return p.parent_path().wstring();
    }

    std::vector<std::wstring> makeCandidatePaths(const std::wstring& relativePath)
    {
        const std::wstring exeDir = getExeDirectory();
        std::vector<std::wstring> candidates;
        candidates.reserve(7);

        // 実行カレント / 実行ファイル直下 / 出力先(x64/Debug)から2階層上(プロジェクトルート)を順に試す。
        candidates.push_back(relativePath);
        candidates.push_back(exeDir + L"\\" + relativePath);
        candidates.push_back(exeDir + L"\\..\\" + relativePath);
        candidates.push_back(exeDir + L"\\..\\..\\" + relativePath);
        candidates.push_back(exeDir + L"\\..\\..\\..\\" + relativePath);
        candidates.push_back(L".\\" + relativePath);
        candidates.push_back(L"..\\" + relativePath);

        return candidates;
    }

    bool resolveExistingPath(const std::wstring& relativePath, std::wstring& outResolvedPath)
    {
        outResolvedPath.clear();

        for (const auto& candidate : makeCandidatePaths(relativePath))
        {
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec))
            {
                outResolvedPath = candidate;
                return true;
            }
        }

        return false;
    }

    std::wstring getPreferredProjectRelativePath(const std::wstring& relativePath)
    {
        const std::wstring exeDir = getExeDirectory();
        return exeDir + L"\\..\\..\\" + relativePath;
    }

    HMODULE loadDxcompilerModule()
    {
        // 1) まず通常ロード（PATH/実行ファイル隣接）
        if (HMODULE mod = LoadLibraryW(L"dxcompiler.dll"))
        {
            return mod;
        }

        // 2) 実行ファイル隣接
        const std::wstring exeDir = getExeDirectory();
        if (HMODULE mod = LoadLibraryW((exeDir + L"\\dxcompiler.dll").c_str()))
        {
            return mod;
        }

        // 3) Windows Kits の最新版 x64/bin を探索
        {
            const std::filesystem::path kitsRoot(L"C:\\Program Files (x86)\\Windows Kits\\10\\bin");
            std::error_code ec;
            if (std::filesystem::exists(kitsRoot, ec))
            {
                std::vector<std::filesystem::path> versionDirs;
                for (const auto& entry : std::filesystem::directory_iterator(kitsRoot, ec))
                {
                    if (!entry.is_directory(ec))
                    {
                        continue;
                    }
                    versionDirs.push_back(entry.path());
                }

                std::sort(versionDirs.begin(), versionDirs.end(), [](const auto& a, const auto& b)
                    {
                        return a.filename().wstring() > b.filename().wstring();
                    });

                for (const auto& dir : versionDirs)
                {
                    const auto candidate = dir / L"x64" / L"dxcompiler.dll";
                    if (std::filesystem::exists(candidate, ec))
                    {
                        if (HMODULE mod = LoadLibraryW(candidate.c_str()))
                        {
                            return mod;
                        }
                    }
                }
            }
        }

        // 4) Visual Studio 2022/2026 系の既知配置（環境差分を吸収）
        const std::array<std::wstring, 6> knownPaths =
        {
            L"C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Tools\\Llvm\\bin\\dxcompiler.dll",
            L"C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Tools\\Llvm\\x64\\bin\\dxcompiler.dll",
            L"C:\\Program Files\\Microsoft Visual Studio\\17\\Community\\VC\\Tools\\Llvm\\bin\\dxcompiler.dll",
            L"C:\\Program Files\\Microsoft Visual Studio\\17\\Community\\VC\\Tools\\Llvm\\x64\\bin\\dxcompiler.dll",
            L"C:\\Program Files\\Microsoft Visual Studio\\18\\BuildTools\\VC\\Tools\\Llvm\\bin\\dxcompiler.dll",
            L"C:\\Program Files\\Microsoft Visual Studio\\17\\BuildTools\\VC\\Tools\\Llvm\\bin\\dxcompiler.dll"
        };

        for (const auto& path : knownPaths)
        {
            if (HMODULE mod = LoadLibraryW(path.c_str()))
            {
                return mod;
            }
        }

        return nullptr;
    }
}

void RayTracingRenderer::initialize()
{
    if (m_initialized)
    {
        return;
    }

    m_initialized = true;
    m_supported = initializeDevice();

    if (!m_supported)
    {
        LOG_WARN("[RayTracingRenderer] DXR はこのデバイスで未対応です");
        return;
    }

    m_width = static_cast<UINT>(DX12::Instance().getScreenWidth());
    m_height = static_cast<UINT>(DX12::Instance().getScreenHeight());

    if (!createOutputResources(m_width, m_height))
    {
        LOG_WARN("[RayTracingRenderer] 出力リソース作成に失敗しました");
        m_supported = false;
        return;
    }

    if (!createGlobalRootSignature())
    {
        LOG_WARN("[RayTracingRenderer] グローバルRootSignature作成に失敗しました");
        m_supported = false;
        return;
    }
}

void RayTracingRenderer::resize(UINT width, UINT height)
{
    if (!m_supported)
    {
        return;
    }

    if (width == 0 || height == 0)
    {
        return;
    }

    if (m_width == width && m_height == height)
    {
        return;
    }

    m_width = width;
    m_height = height;

    m_outputTexture.Reset();
    m_outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    createOutputResources(m_width, m_height);
}

bool RayTracingRenderer::execute(RenderPassContext& context)
{
    if (!m_supported || !m_enabled)
    {
        return false;
    }

    auto* cmd = DX12::Instance().getGraphicsCommandList();
    if (!cmd)
    {
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> cmd4;
    if (FAILED(cmd->QueryInterface(IID_PPV_ARGS(cmd4.GetAddressOf()))))
    {
        return false;
    }

    std::vector<GeometryBuffers> geometries;
    if (!gatherSceneGeometry(geometries) || geometries.empty())
    {
        return false;
    }

    UINT geometryHash = 2166136261u;
    for (const auto& g : geometries)
    {
        geometryHash ^= g.vertexCount + 0x9e3779b9u + (geometryHash << 6) + (geometryHash >> 2);
        geometryHash ^= g.indexCount + 0x9e3779b9u + (geometryHash << 6) + (geometryHash >> 2);
    }

    if (!m_blas || geometryHash != m_lastGeometryHash)
    {
        m_geometryBuffers = std::move(geometries);
        if (!buildBottomLevelAS(cmd4.Get()))
        {
            return false;
        }
        m_lastGeometryHash = geometryHash;
    }

    if (!m_tlas)
    {
        if (!buildTopLevelAS(cmd4.Get()))
        {
            return false;
        }
    }

    if (!createRtPipelineState())
    {
        return false;
    }

    if (!createShaderTable())
    {
        return false;
    }

    if (!m_rtStateObject || !m_shaderTable || !m_outputTexture)
    {
        return false;
    }

    if (m_outputState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    {
        auto toUav = CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputTexture.Get(),
            m_outputState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &toUav);
        m_outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);

    cmd4->SetComputeRootSignature(m_globalRootSignature.Get());
    cmd4->SetComputeRootDescriptorTable(0, DescriptorHeapManager::Instance().getGPUHandle(m_outputUavIndex));
    cmd4->SetComputeRootShaderResourceView(1, m_tlas->GetGPUVirtualAddress());

    cmd4->SetPipelineState1(m_rtStateObject.Get());

    const UINT64 recordSize = alignTo(kShaderIdentifierSize, kShaderRecordAlignment);
    const UINT64 rayGenOffset = 0;
    const UINT64 missOffset = alignTo(rayGenOffset + recordSize, kShaderTableAlignment);
    const UINT64 hitOffset = alignTo(missOffset + recordSize, kShaderTableAlignment);
    const D3D12_GPU_VIRTUAL_ADDRESS tableStart = m_shaderTable->GetGPUVirtualAddress();

    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.RayGenerationShaderRecord.StartAddress = tableStart + rayGenOffset;
    desc.RayGenerationShaderRecord.SizeInBytes = recordSize;

    desc.MissShaderTable.StartAddress = tableStart + missOffset;
    desc.MissShaderTable.SizeInBytes = recordSize;
    desc.MissShaderTable.StrideInBytes = recordSize;

    desc.HitGroupTable.StartAddress = tableStart + hitOffset;
    desc.HitGroupTable.SizeInBytes = recordSize;
    desc.HitGroupTable.StrideInBytes = recordSize;

    desc.Width = std::max(1u, m_width);
    desc.Height = std::max(1u, m_height);
    desc.Depth = 1;

    cmd4->DispatchRays(&desc);

    auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_outputTexture.Get());
    cmd->ResourceBarrier(1, &uavBarrier);

    auto toSrv = CD3DX12_RESOURCE_BARRIER::Transition(
        m_outputTexture.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &toSrv);
    m_outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    (void)context;

    return true;
}

bool RayTracingRenderer::initializeDevice()
{
    auto* device = DX12::Instance().getDevice();
    if (!device)
    {
        return false;
    }

    if (FAILED(device->QueryInterface(IID_PPV_ARGS(m_device5.ReleaseAndGetAddressOf()))))
    {
        return false;
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    if (FAILED(m_device5->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS5,
        &options5,
        sizeof(options5))))
    {
        return false;
    }

    return options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

bool RayTracingRenderer::createOutputResources(UINT width, UINT height)
{
    if (!m_device5)
    {
        return false;
    }

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = m_device5->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(m_outputTexture.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        LOG_HR(hr, "[RayTracingRenderer] RT出力テクスチャ作成失敗");
        return false;
    }

    if (m_outputSrvIndex == UINT_MAX)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        m_outputSrvIndex = DescriptorHeapManager::Instance().createSRV(m_outputTexture.Get(), srvDesc);
    }
    else
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        m_device5->CreateShaderResourceView(m_outputTexture.Get(), &srvDesc, DescriptorHeapManager::Instance().getCPUHandle(m_outputSrvIndex));
        DescriptorHeapManager::Instance().syncToVisible(m_outputSrvIndex);
    }

    if (m_outputUavIndex == UINT_MAX)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = texDesc.Format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        m_outputUavIndex = DescriptorHeapManager::Instance().createUAV(m_outputTexture.Get(), nullptr, uavDesc);
    }
    else
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = texDesc.Format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        m_device5->CreateUnorderedAccessView(m_outputTexture.Get(), nullptr, &uavDesc, DescriptorHeapManager::Instance().getCPUHandle(m_outputUavIndex));
        DescriptorHeapManager::Instance().syncToVisible(m_outputUavIndex);
    }

    return m_outputSrvIndex != UINT_MAX && m_outputUavIndex != UINT_MAX;
}

Microsoft::WRL::ComPtr<ID3D12Resource> RayTracingRenderer::createBuffer(
    UINT64 size,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_RESOURCE_STATES initialState,
    D3D12_HEAP_TYPE heapType) const
{
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);
    CD3DX12_HEAP_PROPERTIES heapProps(heapType);

    HRESULT hr = m_device5->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        initialState,
        nullptr,
        IID_PPV_ARGS(buffer.GetAddressOf()));

    if (FAILED(hr))
    {
        LOG_HR(hr, "[RayTracingRenderer] バッファ作成失敗");
    }

    return buffer;
}

bool RayTracingRenderer::buildBottomLevelAS(ID3D12GraphicsCommandList4* cmd4)
{
    if (!cmd4 || !m_device5)
    {
        return false;
    }

    if (m_geometryBuffers.empty())
    {
        return false;
    }

    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDescs;
    geomDescs.reserve(m_geometryBuffers.size());
    for (const auto& geometry : m_geometryBuffers)
    {
        if (!geometry.vb || !geometry.ib || geometry.vertexCount < 3 || geometry.indexCount < 3)
        {
            continue;
        }

        D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
        geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geomDesc.Triangles.VertexBuffer.StartAddress = geometry.vb->GetGPUVirtualAddress();
        geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
        geomDesc.Triangles.VertexCount = geometry.vertexCount;
        geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geomDesc.Triangles.IndexBuffer = geometry.ib->GetGPUVirtualAddress();
        geomDesc.Triangles.IndexCount = geometry.indexCount;
        geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

        geomDescs.push_back(geomDesc);
    }

    if (geomDescs.empty())
    {
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = static_cast<UINT>(geomDescs.size());
    inputs.pGeometryDescs = geomDescs.data();
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
    m_device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);

    if (prebuild.ResultDataMaxSizeInBytes == 0)
    {
        return false;
    }

    m_blasScratch = createBuffer(
        prebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_HEAP_TYPE_DEFAULT);

    m_blas = createBuffer(
        prebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_HEAP_TYPE_DEFAULT);

    if (!m_blasScratch || !m_blas)
    {
        return false;
    }

    auto scratchToUav = CD3DX12_RESOURCE_BARRIER::Transition(
        m_blasScratch.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmd4->ResourceBarrier(1, &scratchToUav);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = inputs;
    buildDesc.ScratchAccelerationStructureData = m_blasScratch->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = m_blas->GetGPUVirtualAddress();

    cmd4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_blas.Get());
    cmd4->ResourceBarrier(1, &uavBarrier);

    return true;
}

bool RayTracingRenderer::buildTopLevelAS(ID3D12GraphicsCommandList4* cmd4)
{
    if (!cmd4 || !m_device5 || !m_blas)
    {
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = 1;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
    m_device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);

    if (prebuild.ResultDataMaxSizeInBytes == 0)
    {
        return false;
    }

    m_tlasScratch = createBuffer(
        prebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_HEAP_TYPE_DEFAULT);

    m_tlas = createBuffer(
        prebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_HEAP_TYPE_DEFAULT);

    m_instanceDescBuffer = createBuffer(
        sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD);

    if (!m_tlasScratch || !m_tlas || !m_instanceDescBuffer)
    {
        return false;
    }

    auto scratchToUav = CD3DX12_RESOURCE_BARRIER::Transition(
        m_tlasScratch.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmd4->ResourceBarrier(1, &scratchToUav);

    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    instanceDesc.InstanceID = 0;
    instanceDesc.InstanceContributionToHitGroupIndex = 0;
    instanceDesc.InstanceMask = 0xFF;
    instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    instanceDesc.AccelerationStructure = m_blas->GetGPUVirtualAddress();
    instanceDesc.Transform[0][0] = 1.0f;
    instanceDesc.Transform[1][1] = 1.0f;
    instanceDesc.Transform[2][2] = 1.0f;

    void* mapped = nullptr;
    CD3DX12_RANGE range(0, 0);
    if (FAILED(m_instanceDescBuffer->Map(0, &range, &mapped)))
    {
        return false;
    }
    memcpy(mapped, &instanceDesc, sizeof(instanceDesc));
    m_instanceDescBuffer->Unmap(0, nullptr);

    inputs.InstanceDescs = m_instanceDescBuffer->GetGPUVirtualAddress();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = inputs;
    buildDesc.ScratchAccelerationStructureData = m_tlasScratch->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress();

    cmd4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_tlas.Get());
    cmd4->ResourceBarrier(1, &uavBarrier);

    return true;
}

bool RayTracingRenderer::gatherSceneGeometry(std::vector<GeometryBuffers>& outGeometries) const
{
    outGeometries.clear();

    const auto& all = GameObjectRegistry::Instance().getAll();
    for (auto* obj : all)
    {
        if (!obj || !obj->isEnabled() || obj->isDestroyed())
        {
            continue;
        }

        auto* fbx = obj->getComponent<FbxRenderComponent>();
        if (!fbx || !fbx->isEnabled() || !fbx->isActiveInHierarchy())
        {
            continue;
        }

        Model* model = fbx->getModel();
        if (!model || !model->getResource())
        {
            continue;
        }

        const auto& modelData = model->getResource()->getModelData();
        const auto& bones = model->getBone();

        for (const auto& mesh : modelData.meshes)
        {
            if (mesh.vertices.empty() || mesh.indices.empty())
            {
                continue;
            }

            if (mesh.nodeIndex < 0 || static_cast<size_t>(mesh.nodeIndex) >= bones.size())
            {
                continue;
            }

            const Matrix& world = bones[static_cast<size_t>(mesh.nodeIndex)].worldTransform;

            std::vector<Vertex> worldVertices;
            worldVertices.reserve(mesh.vertices.size());
            for (const auto& v : mesh.vertices)
            {
                Vector3 p = Vector3::Transform(v.position, world);
                worldVertices.push_back(Vertex{ p.x, p.y, p.z });
            }

            std::vector<uint32_t> indices = mesh.indices;

            GeometryBuffers geometry{};
            const UINT64 vbSize = static_cast<UINT64>(worldVertices.size()) * sizeof(Vertex);
            const UINT64 ibSize = static_cast<UINT64>(indices.size()) * sizeof(uint32_t);
            geometry.vb = createBuffer(vbSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
            geometry.ib = createBuffer(ibSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
            if (!geometry.vb || !geometry.ib)
            {
                continue;
            }

            void* mappedVB = nullptr;
            void* mappedIB = nullptr;
            CD3DX12_RANGE range(0, 0);
            if (FAILED(geometry.vb->Map(0, &range, &mappedVB)) || FAILED(geometry.ib->Map(0, &range, &mappedIB)))
            {
                if (mappedVB)
                {
                    geometry.vb->Unmap(0, nullptr);
                }
                continue;
            }

            memcpy(mappedVB, worldVertices.data(), static_cast<size_t>(vbSize));
            memcpy(mappedIB, indices.data(), static_cast<size_t>(ibSize));
            geometry.vb->Unmap(0, nullptr);
            geometry.ib->Unmap(0, nullptr);

            geometry.vertexCount = static_cast<UINT>(worldVertices.size());
            geometry.indexCount = static_cast<UINT>(indices.size());
            outGeometries.push_back(std::move(geometry));
        }
    }

    return !outGeometries.empty();
}

bool RayTracingRenderer::createGlobalRootSignature()
{
    if (!m_device5)
    {
        return false;
    }

    CD3DX12_DESCRIPTOR_RANGE uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER params[2] = {};
    params[0].InitAsDescriptorTable(1, &uavRange);
    params[1].InitAsShaderResourceView(0); // TLAS

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        _countof(params),
        params,
        static_cast<UINT>(SamplerState::MAX),
        PiplineState::Instance().getSamplerStates(),
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr))
    {
        if (error)
        {
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        }
        return false;
    }

    hr = m_device5->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(m_globalRootSignature.ReleaseAndGetAddressOf()));

    return SUCCEEDED(hr);
}

bool RayTracingRenderer::loadDxilLibrary(const std::wstring& path, std::vector<uint8_t>& outBinary) const
{
    outBinary.clear();

    std::wstring resolvedPath;
    if (!resolveExistingPath(path, resolvedPath))
    {
        return false;
    }

    std::ifstream ifs(resolvedPath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
    {
        return false;
    }

    std::streamsize size = ifs.tellg();
    if (size <= 0)
    {
        return false;
    }

    ifs.seekg(0, std::ios::beg);
    outBinary.resize(static_cast<size_t>(size));
    return ifs.read(reinterpret_cast<char*>(outBinary.data()), size).good();
}

bool RayTracingRenderer::createRtPipelineState()
{
    if (m_rtStateObject)
    {
        return true;
    }

    if (!m_device5 || !m_globalRootSignature)
    {
        return false;
    }

    m_rtDxilBinary.clear();
    if (!loadDxilLibrary(L"Shader/RT/HybridRays.dxil", m_rtDxilBinary))
    {
        std::wstring hlslPath;
        const bool hasHlsl = resolveExistingPath(L"HLSL/RT/HybridRays.hlsl", hlslPath);
        const bool compiled = hasHlsl && compileDxilLibraryFromHlsl(hlslPath, m_rtDxilBinary);
        if (!compiled)
        {
            if (!m_warnedMissingDxil)
            {
                LOG_WARN("[RayTracingRenderer] Shader/RT/HybridRays.dxil と HLSL/RT/HybridRays.hlsl の両方を読み込めないため DXR Dispatch をスキップします");
                m_warnedMissingDxil = true;
            }
            return false;
        }

        // 次回起動での再コンパイルを避けるためにDXILを保存（失敗しても継続）。
        const std::wstring cachePath = getPreferredProjectRelativePath(L"Shader/RT/HybridRays.dxil");
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(cachePath).parent_path(), ec);
        std::ofstream ofs(cachePath, std::ios::binary | std::ios::trunc);
        if (ofs.is_open())
        {
            ofs.write(reinterpret_cast<const char*>(m_rtDxilBinary.data()), static_cast<std::streamsize>(m_rtDxilBinary.size()));
        }
    }

    D3D12_EXPORT_DESC exports[3] = {};
    exports[0].Name = L"RayGen";
    exports[1].Name = L"Miss";
    exports[2].Name = L"ClosestHit";

    D3D12_DXIL_LIBRARY_DESC libDesc = {};
    D3D12_SHADER_BYTECODE libBytecode = {};
    libBytecode.pShaderBytecode = m_rtDxilBinary.data();
    libBytecode.BytecodeLength = m_rtDxilBinary.size();
    libDesc.DXILLibrary = libBytecode;
    libDesc.NumExports = _countof(exports);
    libDesc.pExports = exports;

    D3D12_HIT_GROUP_DESC hitGroup = {};
    hitGroup.HitGroupExport = L"HitGroup";
    hitGroup.ClosestHitShaderImport = L"ClosestHit";
    hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    shaderConfig.MaxPayloadSizeInBytes = 16;
    shaderConfig.MaxAttributeSizeInBytes = 8;

    LPCWSTR shaderExports[] = { L"RayGen", L"Miss", L"HitGroup" };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderConfigAssociation = {};
    shaderConfigAssociation.pSubobjectToAssociate = nullptr; // 後で設定
    shaderConfigAssociation.NumExports = _countof(shaderExports);
    shaderConfigAssociation.pExports = shaderExports;

    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSig = {};
    globalRootSig.pGlobalRootSignature = m_globalRootSignature.Get();

    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    pipelineConfig.MaxTraceRecursionDepth = 1;

    D3D12_STATE_SUBOBJECT subobjects[6] = {};
    subobjects[0].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    subobjects[0].pDesc = &libDesc;

    subobjects[1].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    subobjects[1].pDesc = &hitGroup;

    subobjects[2].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    subobjects[2].pDesc = &shaderConfig;

    shaderConfigAssociation.pSubobjectToAssociate = &subobjects[2];
    subobjects[3].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    subobjects[3].pDesc = &shaderConfigAssociation;

    subobjects[4].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    subobjects[4].pDesc = &globalRootSig;

    subobjects[5].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    subobjects[5].pDesc = &pipelineConfig;

    D3D12_STATE_OBJECT_DESC stateObjectDesc = {};
    stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    stateObjectDesc.NumSubobjects = _countof(subobjects);
    stateObjectDesc.pSubobjects = subobjects;

    HRESULT hr = m_device5->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(m_rtStateObject.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        LOG_HR(hr, "[RayTracingRenderer] RTPSO 作成失敗");
        return false;
    }

    return true;
}

bool RayTracingRenderer::compileDxilLibraryFromHlsl(const std::wstring& hlslPath, std::vector<uint8_t>& outBinary) const
{
    outBinary.clear();

    HMODULE dxcModule = loadDxcompilerModule();
    if (!dxcModule)
    {
        if (!m_warnedMissingDxc)
        {
            LOG_WARN("[RayTracingRenderer] dxcompiler.dll が見つからないため RT HLSL の実行時コンパイルをスキップします");
            m_warnedMissingDxc = true;
        }
        return false;
    }

    using DxcCreateInstanceProc = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
    auto dxcCreateInstance = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(dxcModule, "DxcCreateInstance"));
    if (!dxcCreateInstance)
    {
        FreeLibrary(dxcModule);
        return false;
    }

    Microsoft::WRL::ComPtr<IDxcUtils> utils;
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;

    HRESULT hr = dxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf()));
    if (FAILED(hr) || !utils)
    {
        FreeLibrary(dxcModule);
        return false;
    }

    hr = dxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf()));
    if (FAILED(hr) || !compiler)
    {
        FreeLibrary(dxcModule);
        return false;
    }

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
    hr = utils->LoadFile(hlslPath.c_str(), nullptr, sourceBlob.GetAddressOf());
    if (FAILED(hr) || !sourceBlob)
    {
        FreeLibrary(dxcModule);
        return false;
    }

    DxcBuffer source = {};
    source.Ptr = sourceBlob->GetBufferPointer();
    source.Size = sourceBlob->GetBufferSize();
    source.Encoding = DXC_CP_UTF8;

    const wchar_t* args[] =
    {
        hlslPath.c_str(),
        L"-T", L"lib_6_6",
        L"-Zi",
        L"-Qembed_debug",
        L"-HV", L"2021",
    };

    Microsoft::WRL::ComPtr<IDxcResult> result;
    hr = compiler->Compile(&source, args, _countof(args), nullptr, IID_PPV_ARGS(result.GetAddressOf()));
    if (FAILED(hr) || !result)
    {
        FreeLibrary(dxcModule);
        return false;
    }

    HRESULT status = S_OK;
    result->GetStatus(&status);
    if (FAILED(status))
    {
        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
        if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr)) && errors && errors->GetStringLength() > 0)
        {
            LOG_ERROR("[RayTracingRenderer] DXC compile error: %s", errors->GetStringPointer());
        }

        FreeLibrary(dxcModule);
        return false;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> objectBlob;
    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(objectBlob.GetAddressOf()), nullptr);
    if (FAILED(hr) || !objectBlob)
    {
        FreeLibrary(dxcModule);
        return false;
    }

    outBinary.resize(objectBlob->GetBufferSize());
    memcpy(outBinary.data(), objectBlob->GetBufferPointer(), objectBlob->GetBufferSize());

    FreeLibrary(dxcModule);
    return true;
}

bool RayTracingRenderer::createShaderTable()
{
    if (m_shaderTable)
    {
        return true;
    }

    if (!m_rtStateObject)
    {
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> props;
    if (FAILED(m_rtStateObject.As(&props)) || !props)
    {
        return false;
    }

    const void* rayGenId = props->GetShaderIdentifier(L"RayGen");
    const void* missId = props->GetShaderIdentifier(L"Miss");
    const void* hitId = props->GetShaderIdentifier(L"HitGroup");
    if (!rayGenId || !missId || !hitId)
    {
        return false;
    }

    const UINT64 recordSize = alignTo(kShaderIdentifierSize, kShaderRecordAlignment);
    const UINT64 rayGenOffset = 0;
    const UINT64 missOffset = alignTo(rayGenOffset + recordSize, kShaderTableAlignment);
    const UINT64 hitOffset = alignTo(missOffset + recordSize, kShaderTableAlignment);
    const UINT64 tableSize = alignTo(hitOffset + recordSize, kShaderTableAlignment);

    m_shaderTable = createBuffer(
        tableSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD);

    if (!m_shaderTable)
    {
        return false;
    }

    void* mapped = nullptr;
    CD3DX12_RANGE range(0, 0);
    if (FAILED(m_shaderTable->Map(0, &range, &mapped)))
    {
        return false;
    }

    uint8_t* dst = reinterpret_cast<uint8_t*>(mapped);
    memcpy(dst + rayGenOffset, rayGenId, kShaderIdentifierSize);
    memcpy(dst + missOffset, missId, kShaderIdentifierSize);
    memcpy(dst + hitOffset, hitId, kShaderIdentifierSize);

    m_shaderTable->Unmap(0, nullptr);
    return true;
}
