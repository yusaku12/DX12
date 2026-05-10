#include "pch.h"
#include "FbxRenderComponent.h"
#include "TransformComponent.h"
#include "Editor/EditorContext.h"

FbxRenderComponent::FbxRenderComponent(const std::string& fbxPath)
{
    // fbxを読み込み
    if (loadFbx(fbxPath))
    {
        LOG_INFO("[FbxRenderComponent] Loaded FBX: %s", fbxPath.c_str());
    }
    else
    {
        return;
    }

    // GPUリソース構築
    buildGPUResources();

    // 統計情報更新
    m_model->getResource()->computeStatistics();
}

void FbxRenderComponent::awake()
{
    m_transform = gameObject()->getComponent<TransformComponent>();
}

void FbxRenderComponent::update()
{
    // Transform が取得できていなければスキップ
    if (!m_transform) return;

    // モデル行列更新
    m_model->updateTransform(m_transform->getLocalMatrix());

    // AABB 描画
    if (g_editor.selectedObject == gameObject())
    {
        renderAABB();
    }
}

void FbxRenderComponent::render()
{
    auto cmd = DX12::Instance().getGraphicsCommandList();
    // コマンドリストが無効なら描画スキップ
    if (!cmd) return;
    render(cmd);
}

void FbxRenderComponent::render(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd) return;

    // デバッグモードに応じた PSO で描画
    size_t psoKey = m_solidPSOKey;
    if (m_debugMode == DebugMode::Wireframe)
    {
        psoKey = m_wireframePSOKey;
    }

    renderInternal(cmd, psoKey);
}

void FbxRenderComponent::renderGBuffer(ID3D12GraphicsCommandList* cmd)
{
    if (!cmd) return;
    renderInternal(cmd, m_gbufferPSOKey);
}

void FbxRenderComponent::inspectGUI()
{
    if (ImGui::BeginTabBar("FBXRender"))
    {
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"統計")))
        {
            imguiStatisticsPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"メッシュ")))
        {
            imguiMeshPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"マテリアル")))
        {
            imguiMaterialPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"ボーン")))
        {
            imguiBonePanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"デバッグ")))
        {
            imguiDebugPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(reinterpret_cast<const char*>(u8"エクスポート")))
        {
            imguiExportPanel();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

bool FbxRenderComponent::getWorldAABB(Vector3& outCenter, Vector3& outExtents) const
{
    if (!m_model) return false;

    Vector3 worldMin(std::numeric_limits<float>::max());
    Vector3 worldMax(std::numeric_limits<float>::lowest());

    const auto& modelData = m_model->getResource()->getModelData();

    // 早期リターン：メッシュが無ければ無効
    if (modelData.meshes.empty()) return false;

    // ヘルパー：1つのワールド空間点で min/max 更新
    auto updateMinMax = [&](const Vector3& p)
        {
            worldMin = Vector3::Min(worldMin, p);
            worldMax = Vector3::Max(worldMax, p);
        };

    for (const auto& mesh : modelData.meshes)
    {
        // メッシュローカルの最小/最大から 8 コーナーを作る
        const Vector3 localMin(mesh.boundsMin.x, mesh.boundsMin.y, mesh.boundsMin.z);
        const Vector3 localMax(mesh.boundsMax.x, mesh.boundsMax.y, mesh.boundsMax.z);

        std::array<Vector3, 8> corners = {
            Vector3(localMin.x, localMin.y, localMin.z),
            Vector3(localMax.x, localMin.y, localMin.z),
            Vector3(localMin.x, localMax.y, localMin.z),
            Vector3(localMax.x, localMax.y, localMin.z),
            Vector3(localMin.x, localMin.y, localMax.z),
            Vector3(localMax.x, localMin.y, localMax.z),
            Vector3(localMin.x, localMax.y, localMax.z),
            Vector3(localMax.x, localMax.y, localMax.z)
        };

        // メッシュのルートノードの worldTransform で変換（スキニングの有無に関わらず）
        const Matrix& nodeWorld = m_model->getBone().at(mesh.nodeIndex).worldTransform;
        for (const auto& c : corners)
        {
            Vector3 w = Vector3::Transform(c, nodeWorld);
            updateMinMax(w);
        }
    }

    // 無効判定
    if (worldMin.x > worldMax.x ||
        worldMin.y > worldMax.y ||
        worldMin.z > worldMax.z)
    {
        return false;
    }

    outCenter = (worldMin + worldMax) * 0.5f;
    outExtents = (worldMax - worldMin) * 0.5f;
    return true;
}

void FbxRenderComponent::renderAABB()
{
    Vector3 center{};
    Vector3 extents{};

    // AABB を取得できなければ描画しない
    if (!getWorldAABB(center, extents))
        return;

    Matrix boxWorld = Matrix::CreateTranslation(center);
    Vector4 color = Vector4{ 1.0f, 1.0f, 0.0f, 1.0f };

    DebugPrimitive::Instance().drawBox(boxWorld, extents, color);
}

bool FbxRenderComponent::loadFbx(const std::string& fbxPath)
{
    auto fbx = std::make_unique<FbxLoad>();

    if (!fbx->load(fbxPath.c_str()))
    {
        LOG_ERROR("[FbxRender] Failed: %s", fbxPath.c_str());
        return false;
    }

    // 所有権をModelに移動
    m_model = std::make_unique<Model>(std::move(fbx));

    return true;
}

void FbxRenderComponent::buildGPUResources()
{
    // モデル行列 CBV
    UINT meshCount = static_cast<UINT>(m_model->getResource()->getModelData().meshes.size());
    m_modelCB = std::make_unique<ConstantBuffer<ModelCB>>(meshCount);
    for (UINT i = 0; i < meshCount; ++i)
    {
        m_modelCB->update(ModelCB{}, i);
    }

    // テクスチャ読み込み
    m_model->getResource()->createTextures();

    // メッシュ構築
    m_model->getResource()->createMesh();

    // マテリアル CBV
    createMaterialCBV();

    // PSO（ソリッド + ワイヤーフレーム + GBuffer）
    m_solidPSOKey = createPSO(RasterizerState::CULL_CLOCKWISE);
    m_wireframePSOKey = createPSO(RasterizerState::WIRE_FRAME);
    m_gbufferPSOKey = createGBufferPSO();
}

void FbxRenderComponent::createMaterialCBV()
{
    const auto& modelData = m_model->getResource()->getModelData();
    UINT matCount = static_cast<UINT>(modelData.materials.size());
    if (matCount == 0)
    {
        matCount = 1;
    }

    // マテリアル CBV をマテリアル数分確保（最低1つ）
    m_materialCB = std::make_unique<ConstantBuffer<MaterialCB>>(matCount);

    for (UINT i = 0; i < matCount; ++i)
    {
        MaterialCB cb{};
        if (i < static_cast<UINT>(modelData.materials.size()))
        {
            const auto& m = modelData.materials[i];
            cb.diffuse = m.diffuseColor;
            cb.pbr = Vector3{ m.metallic, m.roughness, m.ao };
        }
        else
        {
            // マテリアルが無い場合はデフォルト白
            cb.diffuse = Vector4{ 1.f, 1.f, 1.f, 1.f };
            cb.pbr = Vector3{ 1.0f, 1.0f, 1.0f };
        }
        m_materialCB->update(cb, i);
    }
}

void FbxRenderComponent::updateMaterialCBV()
{
    const auto& modelData = m_model->getResource()->getModelData();
    UINT matCount = static_cast<UINT>(modelData.materials.size());

    for (UINT i = 0; i < matCount; ++i)
    {
        MaterialCB cb{};
        cb.diffuse = modelData.materials[i].diffuseColor;
        cb.pbr = Vector3{ modelData.materials[i].metallic, modelData.materials[i].roughness, modelData.materials[i].ao };
        m_materialCB->update(cb, i);
    }
}

std::vector<D3D12_INPUT_ELEMENT_DESC> FbxRenderComponent::getInputLayout()
{
    return {
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHTS",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONES",      0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

size_t FbxRenderComponent::createPSO(RasterizerState rasterizer)
{
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::FBXStandard;
    psoData.vsShaderId = ShaderID::FBXVS;
    psoData.psShaderId = ShaderID::PBRPS;
    psoData.rasterizerState = rasterizer;
    psoData.blendState = BlendState::ALPHA;
    psoData.depthStencilState = DepthStencilState::DEPTH_DEFALT;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout = getInputLayout();
    return PSOCreator::Instance().registerPSO(psoData);
}

size_t FbxRenderComponent::createGBufferPSO()
{
    PSOCreator::PSOData psoData{};
    psoData.rootSignatureType = RootSignatureType::FBXStandard;
    psoData.vsShaderId = ShaderID::FBXVS;
    psoData.psShaderId = ShaderID::GBufferPS;
    psoData.rasterizerState = RasterizerState::CULL_CLOCKWISE;
    psoData.blendState = BlendState::OPAQUE;
    psoData.depthStencilState = DepthStencilState::DEPTH_DEFALT;
    psoData.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoData.inputLayout = getInputLayout();
    psoData.numRenderTargets = GBufferRenderTargets::RenderTargetCount;
    psoData.rtvFormats[0] = GBufferRenderTargets::BaseColorFormat;
    psoData.rtvFormats[1] = GBufferRenderTargets::NormalRoughnessFormat;
    psoData.rtvFormats[2] = GBufferRenderTargets::WorldPosAoFormat;
    return PSOCreator::Instance().registerPSO(psoData);
}

void FbxRenderComponent::renderInternal(ID3D12GraphicsCommandList* cmd, size_t psoKey)
{
    // PSO とルートシグネチャをセット
    DescriptorHeapManager::Instance().setDescriptorHeap(cmd);
    cmd->SetGraphicsRootSignature(RootSignatureManager::Instance().getRootSignature(RootSignatureType::FBXStandard));
    PSOCreator::Instance().setPSO(psoKey, cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetGraphicsRootConstantBufferView(static_cast<int>(CBVType::Camera), CameraManager::Instance().getGPUAddress());

    const auto& modelData = m_model->getResource()->getModelData();

    for (size_t meshIdx = 0; meshIdx < modelData.meshes.size(); ++meshIdx)
    {
        const auto& mesh = modelData.meshes[meshIdx];

        // メッシュ単位の表示制御
        if (!mesh.visible) continue;

        // ModelCB を組み立てる
        ModelCB modelCBData{};

        // メッシュ用定数バッファ更新
        if (!mesh.nodeIndices.empty())
        {
            for (size_t i = 0; i < mesh.nodeIndices.size(); ++i)
            {
                Matrix worldTransform = m_model->getBone().at(mesh.nodeIndices.at(i)).worldTransform;
                Matrix offsetTransform = mesh.offsetTransforms.at(i);
                Matrix boneTransform = offsetTransform * worldTransform;
                modelCBData.boneTransforms[i] = boneTransform;
            }
        }
        else
        {
            modelCBData.boneTransforms[0] = m_model->getBone().at(mesh.nodeIndex).worldTransform;
        }

        // CBV 更新 & ルートにセット
        m_modelCB->update(modelCBData, static_cast<UINT>(meshIdx));
        cmd->SetGraphicsRootConstantBufferView(1, m_modelCB->getGPUAddress(static_cast<UINT>(meshIdx)));

        // IBL ディスクリプタをセット
        auto iblHandle = IBLManager::Instance().getDescriptorHandle();
        if (iblHandle.ptr != 0)
        {
            cmd->SetGraphicsRootDescriptorTable(4, iblHandle);
        }

        // メッシュバッファをセット（現在処理中のメッシュのみ）
        m_model->getResource()->bindGpuMesh(cmd, meshIdx);

        for (const auto& subset : mesh.subMeshes)
        {
            // サブセット単位の表示制御
            if (!subset.visible) continue;

            // マテリアル単位の表示制御
            if (subset.materialIndex < modelData.materials.size())
            {
                if (!modelData.materials[subset.materialIndex].visible)
                    continue;
            }

            // descriptorBase が無効ならそのサブセットは描画スキップ（不正なハンドルを渡さない）
            if (subset.descriptorBase == UINT_MAX)
            {
                LOG_WARN("[FbxRenderComponent] Skip subset with invalid descriptorBase");
                continue;
            }

            // material index が範囲外なら 0 にフォールバック
            UINT matIndex = 0;
            if (subset.materialIndex < modelData.materials.size())
                matIndex = static_cast<UINT>(subset.materialIndex);

            cmd->SetGraphicsRootConstantBufferView(2, m_materialCB->getGPUAddress(matIndex));
            cmd->SetGraphicsRootDescriptorTable(3, DescriptorHeapManager::Instance().getGPUHandle(subset.descriptorBase));
            cmd->DrawIndexedInstanced(subset.indexCount, 1, subset.startIndex, 0, 0);
        }
    }
}

void FbxRenderComponent::imguiStatisticsPanel()
{
    ImGui::Columns(2, "StatsColumns", true);
    ImGui::SetColumnWidth(0, 150.0f);

    auto& status = m_model->getResource()->getStatistics();
    ImGui::Text(reinterpret_cast<const char*>(u8"メッシュ数"));        ImGui::NextColumn(); ImGui::Text("%u", status.meshCount);        ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"マテリアル数"));      ImGui::NextColumn(); ImGui::Text("%u", status.materialCount);    ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"サブメッシュ数"));    ImGui::NextColumn(); ImGui::Text("%u", status.subMeshCount);     ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"頂点数"));            ImGui::NextColumn(); ImGui::Text("%u", status.totalVertices);    ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"インデックス数"));    ImGui::NextColumn(); ImGui::Text("%u", status.totalIndices);     ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"三角形数"));          ImGui::NextColumn(); ImGui::Text("%u", status.totalTriangles);   ImGui::NextColumn();
    ImGui::Text(reinterpret_cast<const char*>(u8"ドローコール数"));    ImGui::NextColumn(); ImGui::Text("%u", status.drawCallCount);    ImGui::NextColumn();

    ImGui::Columns(1);
}

void FbxRenderComponent::imguiMeshPanel()
{
    auto& modelData = m_model->getResource()->getModelData();

    for (size_t i = 0; i < modelData.meshes.size(); ++i)
    {
        auto& meshData = modelData.meshes[i];

        std::string label = std::format("[{}] {}", i, meshData.name.empty() ? "Unnamed" : meshData.name);

        bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

        // 表示 ON/OFF チェックボックス（ツリーノード横に配置）
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30.0f);
        std::string checkId = std::format("##meshVis{}", i);
        ImGui::Checkbox(checkId.c_str(), &meshData.visible);

        if (nodeOpen)
        {
            ImGui::Text(reinterpret_cast<const char*>(u8"  頂点数: %zu"), meshData.vertices.size());
            ImGui::Text(reinterpret_cast<const char*>(u8"  インデックス数: %zu"), meshData.indices.size());
            ImGui::Text(reinterpret_cast<const char*>(u8"  サブメッシュ数: %zu"), meshData.subMeshes.size());

            // サブセット詳細
            for (size_t j = 0; j < meshData.subMeshes.size(); ++j)
            {
                auto& subset = meshData.subMeshes[j];
                std::string subLabel = std::format("  Subset [{}] mat={} idx={}-{}",
                    j, subset.materialIndex, subset.startIndex, subset.startIndex + subset.indexCount);

                ImGui::Indent();
                std::string subCheckId = std::format("##subVis{}_{}", i, j);
                ImGui::Checkbox(subCheckId.c_str(), &subset.visible);
                ImGui::SameLine();
                ImGui::TextDisabled("%s", subLabel.c_str());
                ImGui::Unindent();
            }

            ImGui::TreePop();
        }
    }
}

void FbxRenderComponent::imguiMaterialPanel()
{
    auto& modelData = m_model->getResource()->getModelData();

    if (modelData.materials.empty())
    {
        ImGui::TextDisabled(reinterpret_cast<const char*>(u8"マテリアルなし"));
        return;
    }

    // プレビューサイズ
    static constexpr float TEX_PREVIEW_SIZE = 64.0f;

    bool materialChanged = false;

    for (size_t i = 0; i < modelData.materials.size(); ++i)
    {
        auto& mat = modelData.materials[i];

        std::string label = std::format("[{}] {}", i, mat.name.empty() ? "Unnamed" : mat.name);

        bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

        // 表示 ON/OFF チェックボックス
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30.0f);
        std::string checkId = std::format("##matVis{}", i);
        ImGui::Checkbox(checkId.c_str(), &mat.visible);

        if (nodeOpen)
        {
            // ディフューズカラー編集（RGBA）
            std::string colorId = std::string(reinterpret_cast<const char*>(u8"ディフューズ##matColor")) + std::to_string(i);
            float color[4] = { mat.diffuseColor.x, mat.diffuseColor.y, mat.diffuseColor.z, mat.diffuseColor.w };
            if (ImGui::ColorEdit4(colorId.c_str(), color, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar))
            {
                mat.diffuseColor = Vector4(color[0], color[1], color[2], color[3]);
                materialChanged = true;
            }

            // PBR パラメータ
            std::string metallicId = std::string(reinterpret_cast<const char*>(u8"メタリック##matMetal")) + std::to_string(i);
            if (ImGui::SliderFloat(metallicId.c_str(), &mat.metallic, 0.0f, 1.0f))
            {
                materialChanged = true;
            }

            std::string roughnessId = std::string(reinterpret_cast<const char*>(u8"ラフネス##matRough")) + std::to_string(i);
            if (ImGui::SliderFloat(roughnessId.c_str(), &mat.roughness, 0.0f, 1.0f))
            {
                materialChanged = true;
            }

            std::string aoId = std::string(reinterpret_cast<const char*>(u8"AO##matAO")) + std::to_string(i);
            if (ImGui::SliderFloat(aoId.c_str(), &mat.ao, 0.0f, 1.0f))
            {
                materialChanged = true;
            }

            // テクスチャ表示＆差し替え
            for (auto texType : magic_enum::enum_values<TextureType>())
            {
                if (texType == TextureType::Max) continue;

                UINT t = static_cast<UINT>(texType);
                auto texTypeName = magic_enum::enum_name(texType);

                ImGui::PushID(static_cast<int>(i * static_cast<UINT>(TextureType::Max) + t));

                ImGui::Separator();
                ImGui::Text("  %.*s:", static_cast<int>(texTypeName.size()), texTypeName.data());

                // テクスチャ差し替え用ラムダ
                auto openAndReplace = [&]()
                    {
                        std::vector<std::wstring> selectedFiles;
                        DialogResult result = Dialog::openFile(
                            selectedFiles,
                            L"Select Texture",
                            L"",
                            false
                        );
                        if (result == DialogResult::OK && !selectedFiles.empty())
                        {
                            m_model->getResource()->replaceTexture(i, texType, selectedFiles[0]);
                        }
                    };

                // コンテキストメニューID
                std::string ctxId = "##texCtx" + std::to_string(t);
                bool hasTexture = !mat.textureName[t].empty();

                // テクスチャプレビュー表示
                LoadTexture* tex = m_model->getResource()->getMaterialTexture(i, texType);
                if (tex && tex->isValid())
                {
                    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = tex->getGPUHandle();
                    if (gpuHandle.ptr != 0)
                    {
                        ImTextureID texID = (ImTextureID)gpuHandle.ptr;

                        // 左クリック → テクスチャ変更
                        if (ImGui::ImageButton("##texBtn", texID, ImVec2(TEX_PREVIEW_SIZE, TEX_PREVIEW_SIZE)))
                        {
                            openAndReplace();
                        }

                        // ホバー時に拡大プレビュー + 操作ヒント
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Image(texID, ImVec2(TEX_PREVIEW_SIZE * 4.0f, TEX_PREVIEW_SIZE * 4.0f));
                            if (hasTexture)
                            {
                                ImGui::Text("%s", mat.textureName[t].c_str());
                            }
                            ImGui::Separator();
                            ImGui::TextDisabled(reinterpret_cast<const char*>(u8"左クリック: 変更 / 右クリック: メニュー"));
                            ImGui::EndTooltip();
                        }

                        // 右クリック → コンテキストメニュー
                        if (ImGui::BeginPopupContextItem(ctxId.c_str()))
                        {
                            if (ImGui::MenuItem(reinterpret_cast<const char*>(u8"変更")))
                            {
                                openAndReplace();
                            }
                            if (hasTexture)
                            {
                                if (ImGui::MenuItem(reinterpret_cast<const char*>(u8"削除")))
                                {
                                    m_model->getResource()->clearTexture(i, texType);
                                }
                                ImGui::Separator();
                                if (ImGui::MenuItem(reinterpret_cast<const char*>(u8"パスをコピー")))
                                {
                                    ImGui::SetClipboardText(mat.textureName[t].c_str());
                                }
                            }
                            ImGui::EndPopup();
                        }
                    }
                }
                else
                {
                    // テクスチャがない場合はプレースホルダーボタン
                    if (ImGui::Button("##texPlaceholder", ImVec2(TEX_PREVIEW_SIZE, TEX_PREVIEW_SIZE)))
                    {
                        openAndReplace();
                    }
                    ImVec2 rectMin = ImGui::GetItemRectMin();
                    ImVec2 rectMax = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(rectMin, rectMax, IM_COL32(128, 128, 128, 255));
                    ImGui::GetWindowDrawList()->AddText(
                        ImVec2(rectMin.x + 8.0f, rectMin.y + TEX_PREVIEW_SIZE * 0.5f - 7.0f),
                        IM_COL32(160, 160, 160, 255), "+ Add");

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip(reinterpret_cast<const char*>(u8"クリックでテクスチャを追加"));
                    }
                }

                // ファイル名表示
                if (hasTexture)
                {
                    ImGui::SameLine();
                    std::filesystem::path texPath(mat.textureName[t]);
                    ImGui::Text("%s", texPath.filename().string().c_str());
                }

                ImGui::PopID();
            }

            ImGui::TreePop();
        }
    }

    // マテリアル CBV を即時更新
    if (materialChanged)
    {
        updateMaterialCBV();
    }
}

void FbxRenderComponent::imguiBonePanel()
{
    const auto& modelData = m_model->getResource()->getModelData();
    const auto& bones = modelData.bones;

    if (bones.empty())
    {
        ImGui::TextDisabled(reinterpret_cast<const char*>(u8"ボーンなし"));
        return;
    }

    ImGui::Text(reinterpret_cast<const char*>(u8"ボーン数: %zu"), bones.size());
    ImGui::Separator();

    // 親子関係マップを構築
    std::vector<std::vector<int>> childMap(bones.size());
    std::vector<int> roots;
    for (int i = 0; i < static_cast<int>(bones.size()); ++i)
    {
        int parent = bones[i].parentIndex;
        if (parent >= 0 && parent < static_cast<int>(bones.size()))
        {
            childMap[parent].push_back(i);
        }
        else
        {
            roots.push_back(i);
        }
    }

    // ルートボーンからツリーを描画
    for (int rootIdx : roots)
    {
        imguiBoneTreeNode(rootIdx, bones, childMap);
    }
}

void FbxRenderComponent::imguiBoneTreeNode(int boneIndex, const std::vector<ModelResource::Bone>& bones,
    const std::vector<std::vector<int>>& childMap)
{
    const auto& bone = bones[boneIndex];
    const auto& runtimeBones = m_model->getBone();

    std::string label = std::format("[{}] {}", boneIndex, bone.name.empty() ? "Unnamed" : bone.name);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (childMap[boneIndex].empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

    // ツールチップで詳細情報表示
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("ID: %llu", bone.id);
        ImGui::Text("Parent: %d", bone.parentIndex);
        ImGui::Text("Scale:     (%.3f, %.3f, %.3f)", bone.scale.x, bone.scale.y, bone.scale.z);
        ImGui::Text("Rotate:    (%.3f, %.3f, %.3f, %.3f)", bone.rotate.x, bone.rotate.y, bone.rotate.z, bone.rotate.w);
        ImGui::Text("Translate: (%.3f, %.3f, %.3f)", bone.translate.x, bone.translate.y, bone.translate.z);

        // ランタイムボーンのワールド行列も表示
        if (boneIndex < static_cast<int>(runtimeBones.size()))
        {
            const auto& rt = runtimeBones[boneIndex];
            const Matrix& w = rt.worldTransform;
            ImGui::Separator();
            ImGui::Text(reinterpret_cast<const char*>(u8"ワールド行列:"));
            ImGui::Text("  [%.2f, %.2f, %.2f, %.2f]", w._11, w._12, w._13, w._14);
            ImGui::Text("  [%.2f, %.2f, %.2f, %.2f]", w._21, w._22, w._23, w._24);
            ImGui::Text("  [%.2f, %.2f, %.2f, %.2f]", w._31, w._32, w._33, w._34);
            ImGui::Text("  [%.2f, %.2f, %.2f, %.2f]", w._41, w._42, w._43, w._44);
        }

        ImGui::EndTooltip();
    }

    // 子ノードがある場合のみ再帰
    if (nodeOpen && !childMap[boneIndex].empty())
    {
        for (int childIdx : childMap[boneIndex])
        {
            imguiBoneTreeNode(childIdx, bones, childMap);
        }
        ImGui::TreePop();
    }
}

void FbxRenderComponent::imguiDebugPanel()
{
    auto names = magic_enum::enum_names<DebugMode>();
    int currentMode = static_cast<int>(m_debugMode);

    if (ImGui::Combo(reinterpret_cast<const char*>(u8"デバッグモード"), &currentMode,
        [](void* data, int idx, const char** out_text)
        {
            auto* arr = static_cast<const decltype(names)*>(data);
            *out_text = (*arr)[idx].data();
            return true;
        },
        (void*)&names,
        (int)names.size()))
    {
        m_debugMode = static_cast<DebugMode>(currentMode);
    }
}

void FbxRenderComponent::imguiExportPanel()
{
    ImGui::TextDisabled(reinterpret_cast<const char*>(u8"エクスポート機能は準備中です"));
}