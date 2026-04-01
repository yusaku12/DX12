#include "pch.h"
#include "FbxLoad.h"

//! FbxDouble2 → XMFLOAT2
inline Vector2 FbxDouble2ToFloat2(const FbxDouble2& fbxValue)
{
    return Vector2
    (
        static_cast<float>(fbxValue[0]),
        static_cast<float>(fbxValue[1])
    );
}

//! FbxDouble3 → XMFLOAT3
inline Vector3 FbxDouble3ToFloat3(const FbxDouble3& fbxValue)
{
    return Vector3
    (
        static_cast<float>(fbxValue[0]),
        static_cast<float>(fbxValue[1]),
        static_cast<float>(fbxValue[2])
    );
}

//! FbxDouble4 → XMFLOAT3
inline Vector3 FbxDouble4ToFloat3(const FbxDouble4& fbxValue)
{
    return Vector3
    (
        static_cast<float>(fbxValue[0]),
        static_cast<float>(fbxValue[1]),
        static_cast<float>(fbxValue[2])
    );
}

//! FbxDouble4 → XMFLOAT4
inline Vector4 FbxDouble4ToFloat4(const FbxDouble4& fbxValue)
{
    return Vector4
    (
        static_cast<float>(fbxValue[0]),
        static_cast<float>(fbxValue[1]),
        static_cast<float>(fbxValue[2]),
        static_cast<float>(fbxValue[3])
    );
}

//! FbxDouble4 → XMFLOAT4
inline Matrix FbxAMatrixToFloat4x4(const FbxAMatrix& fbxValue)
{
    return Matrix
    (
        static_cast<float>(fbxValue[0][0]),
        static_cast<float>(fbxValue[0][1]),
        static_cast<float>(fbxValue[0][2]),
        static_cast<float>(fbxValue[0][3]),
        static_cast<float>(fbxValue[1][0]),
        static_cast<float>(fbxValue[1][1]),
        static_cast<float>(fbxValue[1][2]),
        static_cast<float>(fbxValue[1][3]),
        static_cast<float>(fbxValue[2][0]),
        static_cast<float>(fbxValue[2][1]),
        static_cast<float>(fbxValue[2][2]),
        static_cast<float>(fbxValue[2][3]),
        static_cast<float>(fbxValue[3][0]),
        static_cast<float>(fbxValue[3][1]),
        static_cast<float>(fbxValue[3][2]),
        static_cast<float>(fbxValue[3][3])
    );
}

bool FbxLoad::load(const char* filename)
{
    // ディレクトリパス取得
    char drive[32], dir[256], dirname[256];
    ::_splitpath_s(filename, drive, sizeof(drive), dir, sizeof(dir), nullptr, 0, nullptr, 0);
    ::_makepath_s(dirname, sizeof(dirname), drive, dir, nullptr, nullptr);

    const char* ext = strrchr(filename, '.');
    if (::_stricmp(ext, ".fbx") == 0)
    {
        // FBXのファイルパスはUTF-8にする必要がある
        char fbxFilename[256];
        stringToUTF8(filename, fbxFilename, sizeof(fbxFilename));

        FbxManager* fbxManager = FbxManager::Create();

        // FBXに対する入出力を定義する
        FbxIOSettings* fbxIOS = FbxIOSettings::Create(fbxManager, IOSROOT);	// 特別な理由がない限りIOSROOTを指定
        fbxManager->SetIOSettings(fbxIOS);

        // インポータを生成
        FbxImporter* fbxImporter = FbxImporter::Create(fbxManager, "");
        bool result = fbxImporter->Initialize(fbxFilename, -1, fbxManager->GetIOSettings());	// -1でファイルフォーマット自動判定
        LOG_ASSERT(result, "FbxImporter::Initialize() : Failed!!\n");

        // SceneオブジェクトにFBXファイル内の情報を流し込む
        FbxScene* fbxScene = FbxScene::Create(fbxManager, "scene");
        fbxImporter->Import(fbxScene);
        fbxImporter->Destroy();	// シーンを流し込んだらImporterは解放してOK

        // ジオメトリを三角形化しておく
        FbxGeometryConverter fbxGeometryConverter(fbxManager);
        fbxGeometryConverter.Triangulate(fbxScene, true);
        fbxGeometryConverter.RemoveBadPolygonsFromMeshes(fbxScene);

        // 軸がZ-Upの場合はY-Upに変換する
        FbxAxisSystem fbx_scene_axis_system = fbxScene->GetGlobalSettings().GetAxisSystem();
        if (fbx_scene_axis_system == FbxAxisSystem::eMayaZUp)
        {
            FbxAxisSystem::MayaYUp.ConvertScene(fbxScene);
        }

        // 単位をメートルに変換する
        FbxSystemUnit sceneUnit = fbxScene->GetGlobalSettings().GetSystemUnit();
        if (sceneUnit != FbxSystemUnit::m)
        {
            FbxSystemUnit::m.ConvertScene(fbxScene);
        }

        // モデル読み込み
        std::vector<FbxNode*> fbxNodes;
        FbxNode* fbxRootNode = fbxScene->GetRootNode();
        loadMaterials(dirname, fbxScene);
        loadNodes(fbxRootNode, -1);
        loadMeshes(fbxRootNode);

        // アニメーション読み込み
        char name[256];
        ::_splitpath_s(filename, nullptr, 0, nullptr, 0, name, 256, nullptr, 0);

        loadAnimations(fbxScene, nullptr, false);

        // マネージャ解放
        fbxManager->Destroy();		// 関連するすべてのオブジェクトが解放される
    }

    return true;
}

void FbxLoad::loadNodes(FbxNode* fbxNode, int parentNodeIndex)
{
    FbxNodeAttribute* fbxNodeAttribute = fbxNode->GetNodeAttribute();
    FbxNodeAttribute::EType fbxNodeAttributeType = FbxNodeAttribute::EType::eUnknown;

    if (fbxNodeAttribute != nullptr)
    {
        fbxNodeAttributeType = fbxNodeAttribute->GetAttributeType();
    }

    switch (fbxNodeAttributeType)
    {
    case FbxNodeAttribute::eUnknown:
    case FbxNodeAttribute::eNull:
    case FbxNodeAttribute::eMarker:
    case FbxNodeAttribute::eMesh:
    case FbxNodeAttribute::eSkeleton:
        loadNode(fbxNode, parentNodeIndex);
        break;
    }

    // 再帰的に子ノードを処理する
    parentNodeIndex = static_cast<int>(m_model.bones.size() - 1);
    for (int i = 0; i < fbxNode->GetChildCount(); ++i)
    {
        loadNodes(fbxNode->GetChild(i), parentNodeIndex);
    }
}

void FbxLoad::loadNode(FbxNode* fbxNode, int parentNodeIndex)
{
    FbxAMatrix fbxLocalTransform = fbxNode->EvaluateLocalTransform();

    Bone node;
    node.id = getNodeId(fbxNode);
    node.name = fbxNode->GetName();
    node.parentIndex = parentNodeIndex;
    node.scale = FbxDouble4ToFloat3(fbxLocalTransform.GetS());
    node.rotate = FbxDouble4ToFloat4(fbxLocalTransform.GetQ());
    node.translate = FbxDouble4ToFloat3(fbxLocalTransform.GetT());

    // 座標軸変換
    convertTranslationFromRHtoLH(node.translate);
    convertRotationFromRHtoLH(node.rotate);

    m_model.bones.push_back(node);
}

void FbxLoad::loadMeshes(FbxNode* fbxNode)
{
    FbxNodeAttribute* fbxNodeAttribute = fbxNode->GetNodeAttribute();
    FbxNodeAttribute::EType fbxNodeAttributeType = FbxNodeAttribute::EType::eUnknown;

    if (fbxNodeAttribute != nullptr)
    {
        fbxNodeAttributeType = fbxNodeAttribute->GetAttributeType();
    }

    switch (fbxNodeAttributeType)
    {
    case FbxNodeAttribute::eMesh:
        loadMesh(fbxNode, static_cast<FbxMesh*>(fbxNodeAttribute));
        break;
    }

    // 再帰的に子ノードを処理する
    for (int i = 0; i < fbxNode->GetChildCount(); ++i)
    {
        loadMeshes(fbxNode->GetChild(i));
    }
}

void FbxLoad::loadMesh(FbxNode* fbxNode, FbxMesh* fbxMesh)
{
    int fbxControlPointsCount = fbxMesh->GetControlPointsCount();

    int fbxMaterialCount = fbxNode->GetMaterialCount();
    int fbxPolygonCount = fbxMesh->GetPolygonCount();

    m_model.meshes.emplace_back(Mesh());
    Mesh& mesh = m_model.meshes.back();
    UINT64 nodeId = getNodeId(fbxNode);
    mesh.nodeIndex = findNodeIndex(nodeId);
    mesh.subMeshes.resize(fbxMaterialCount > 0 ? fbxMaterialCount : 1);
    mesh.boundsMin.x = mesh.boundsMin.y = mesh.boundsMin.z = FLT_MAX;
    mesh.boundsMax.x = mesh.boundsMax.y = mesh.boundsMax.z = -FLT_MAX;
    mesh.name = fbxNode->GetName();

    // サブセットのマテリアル設定
    for (int fbxMaterialIndex = 0; fbxMaterialIndex < fbxMaterialCount; ++fbxMaterialIndex)
    {
        const FbxSurfaceMaterial* fbxSurfaceMaterial = fbxNode->GetMaterial(fbxMaterialIndex);

        SubMesh& subset = mesh.subMeshes.at(fbxMaterialIndex);
        subset.materialIndex = findMaterialIndex(fbxNode->GetScene(), fbxSurfaceMaterial);
    }

    // サブセットの頂点インデックス範囲設定
    if (fbxMaterialCount > 0)
    {
        for (int fbxPolygonIndex = 0; fbxPolygonIndex < fbxPolygonCount; ++fbxPolygonIndex)
        {
            int fbxMaterialIndex = fbxMesh->GetElementMaterial()->GetIndexArray().GetAt(fbxPolygonIndex);
            mesh.subMeshes.at(fbxMaterialIndex).indexCount += 3;
        }

        int offset = 0;
        for (SubMesh& subset : mesh.subMeshes)
        {
            subset.startIndex = offset;
            offset += subset.indexCount;

            subset.indexCount = 0;
        }
    }

    // 頂点影響力データ
    struct BoneInfluence
    {
        int useCount = 0;
        int indices[4] = { 0, 0, 0, 0 };
        float weights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };

        void Add(int index, float weight)
        {
            if (useCount < 4)
            {
                indices[useCount] = index;
                weights[useCount] = weight;
                useCount++;
            }
        }
    };
    // 頂点影響力データを抽出する
    std::vector<BoneInfluence> boneInfluences;
    {
        boneInfluences.resize(fbxControlPointsCount);

        // スキニングに必要な情報を取得する
        int fbxDeformerCount = fbxMesh->GetDeformerCount(FbxDeformer::eSkin);
        for (int fbxDeformerIndex = 0; fbxDeformerIndex < fbxDeformerCount; ++fbxDeformerIndex)
        {
            FbxSkin* fbxSkin = static_cast<FbxSkin*>(fbxMesh->GetDeformer(fbxDeformerIndex, FbxDeformer::eSkin));

            int fbxClusterCount = fbxSkin->GetClusterCount();
            for (int fbxClusterIndex = 0; fbxClusterIndex < fbxClusterCount; ++fbxClusterIndex)
            {
                FbxCluster* fbxCluster = fbxSkin->GetCluster(fbxClusterIndex);

                // 頂点影響力データを抽出する
                {
                    int fbxClusterControlPointIndicesCount = fbxCluster->GetControlPointIndicesCount();
                    const int* fbxControlPointIndices = fbxCluster->GetControlPointIndices();
                    const double* fbxControlPointWeights = fbxCluster->GetControlPointWeights();

                    for (int i = 0; i < fbxClusterControlPointIndicesCount; ++i)
                    {
                        BoneInfluence& boneInfluence = boneInfluences.at(fbxControlPointIndices[i]);
                        boneInfluence.Add(fbxClusterIndex, static_cast<float>(fbxControlPointWeights[i]));
                    }
                }

                // メッシュ空間からボーン空間への変換行列の計算をする
                {
                    // メッシュ空間からグローバル空間への変換行列
                    FbxAMatrix fbxMeshToGlobalSpaceTransform;
                    fbxCluster->GetTransformMatrix(fbxMeshToGlobalSpaceTransform);

                    // ボーン空間からグローバル空間への変換行列
                    FbxAMatrix fbxBoneToGlobalSpaceTransform;
                    fbxCluster->GetTransformLinkMatrix(fbxBoneToGlobalSpaceTransform);

                    // メッシュ空間からボーン空間への変換行列
                    FbxAMatrix fbxMeshToBoneTransform = fbxBoneToGlobalSpaceTransform.Inverse() * fbxMeshToGlobalSpaceTransform;

                    // 座標系変換
                    Matrix offsetTransform = FbxAMatrixToFloat4x4(fbxMeshToBoneTransform);
                    convertMatrixFromRHtoLH(offsetTransform);

                    mesh.offsetTransforms.emplace_back(offsetTransform);

                    // 影響するボーンインデックスを名前で検索する
                    UINT64 influenodeId = getNodeId(fbxCluster->GetLink());
                    int nodeIndex = findNodeIndex(influenodeId);
                    mesh.nodeIndices.emplace_back(nodeIndex);
                }
            }
        }
    }

    // ジオメトリ変換行列
    FbxAMatrix fbxGeometricTransform
    (
        fbxNode->GetGeometricTranslation(FbxNode::eSourcePivot),
        fbxNode->GetGeometricRotation(FbxNode::eSourcePivot),
        fbxNode->GetGeometricScaling(FbxNode::eSourcePivot)
    );
    Matrix GM = Matrix(FbxAMatrixToFloat4x4(fbxGeometricTransform));

    // UVセット名
    FbxStringList fbxUVSetNames;
    fbxMesh->GetUVSetNames(fbxUVSetNames);

    // 頂点データ
    mesh.vertices.resize(fbxPolygonCount * 3);
    mesh.indices.resize(fbxPolygonCount * 3);

    int vertexCount = 0;
    const FbxVector4* fbxControlPoints = fbxMesh->GetControlPoints();
    for (int fbxPolygonIndex = 0; fbxPolygonIndex < fbxPolygonCount; ++fbxPolygonIndex)
    {
        // ポリゴンに適用されているマテリアルインデックスを取得する
        int fbxMaterialIndex = 0;
        if (fbxMaterialCount > 0)
        {
            fbxMaterialIndex = fbxMesh->GetElementMaterial()->GetIndexArray().GetAt(fbxPolygonIndex);
        }

        SubMesh& subset = mesh.subMeshes.at(fbxMaterialIndex);
        const int indexOffset = subset.startIndex + subset.indexCount;

        for (int fbxVertexIndex = 0; fbxVertexIndex < 3; ++fbxVertexIndex)
        {
            Vertex vertex;

            int fbxControlPointIndex = fbxMesh->GetPolygonVertex(fbxPolygonIndex, fbxVertexIndex);
            // Position
            {
                Vector3 position = FbxDouble4ToFloat3(fbxControlPoints[fbxControlPointIndex]);
                position = Vector3::Transform(position, GM);
                vertex.position = position;
            }

            // Weight
            {
                BoneInfluence& boneInfluence = boneInfluences.at(fbxControlPointIndex);
                vertex.boneIndices.x = boneInfluence.indices[0];
                vertex.boneIndices.y = boneInfluence.indices[1];
                vertex.boneIndices.z = boneInfluence.indices[2];
                vertex.boneIndices.w = boneInfluence.indices[3];
                vertex.boneWeights.x = boneInfluence.weights[0];
                vertex.boneWeights.y = boneInfluence.weights[1];
                vertex.boneWeights.z = boneInfluence.weights[2];
                vertex.boneWeights.w = boneInfluence.weights[3];
            }

            // Normal
            if (fbxMesh->GetElementNormalCount() > 0)
            {
                FbxVector4 fbxNormal;
                fbxMesh->GetPolygonVertexNormal(fbxPolygonIndex, fbxVertexIndex, fbxNormal);
                Vector3 normal = FbxDouble4ToFloat3(fbxNormal);
                normal = Vector3::TransformNormal(normal, GM);
                normal.Normalize();
                vertex.normal = normal;
            }
            else
            {
                vertex.normal = DirectX::XMFLOAT3(0, 0, 0);
            }

            // UV
            if (fbxMesh->GetElementUVCount() > 0)
            {
                bool fbxUnmappedUV;
                FbxVector2 fbxUV;
                fbxMesh->GetPolygonVertexUV(fbxPolygonIndex, fbxVertexIndex, fbxUVSetNames[0], fbxUV, fbxUnmappedUV);
                fbxUV[1] = 1.0 - fbxUV[1];
                vertex.uv = FbxDouble2ToFloat2(fbxUV);
            }
            else
            {
                vertex.uv = DirectX::XMFLOAT2(0, 0);
            }

            // 座標系変換
            convertTranslationFromRHtoLH(vertex.position);
            convertTranslationFromRHtoLH(vertex.normal);

            // 頂点＆インデックスデータ設定
            mesh.indices.at(indexOffset + fbxVertexIndex) = vertexCount;
            mesh.vertices.at(vertexCount) = vertex;
            vertexCount++;

            // バウンディングボックス
            mesh.boundsMin.x = std::fminf(mesh.boundsMin.x, vertex.position.x);
            mesh.boundsMin.y = std::fminf(mesh.boundsMin.y, vertex.position.y);
            mesh.boundsMin.z = std::fminf(mesh.boundsMin.z, vertex.position.z);
            mesh.boundsMax.x = std::fmaxf(mesh.boundsMax.x, vertex.position.x);
            mesh.boundsMax.y = std::fmaxf(mesh.boundsMax.y, vertex.position.y);
            mesh.boundsMax.z = std::fmaxf(mesh.boundsMax.z, vertex.position.z);
        }

        subset.indexCount += 3;
    }

    // タンジェント
    fbxMesh->GenerateTangentsData(0, true);
    fbxsdk::FbxGeometryElementTangent* fbxElementTangent = fbxMesh->GetElementTangent(0);
    if (fbxElementTangent != nullptr)
    {
        int fbxPolygonVertexCount = fbxMesh->GetPolygonVertexCount();
        for (int i = 0; i < fbxPolygonVertexCount; i++)
        {
            FbxVector4 fbxTangent = fbxElementTangent->GetDirectArray().GetAt(i);
            mesh.vertices[i].tangent.x = static_cast<float>(fbxTangent[0]);
            mesh.vertices[i].tangent.y = static_cast<float>(fbxTangent[1]);
            mesh.vertices[i].tangent.z = static_cast<float>(fbxTangent[2]);

            // 座標系変換
            convertTranslationFromRHtoLH(mesh.vertices[i].tangent);
        }
    }

    // 座標系変換
    convertIndexBufferFromRHtoLH(mesh.indices);
}

void FbxLoad::loadMaterials(const char* dirname, FbxScene* fbxScene)
{
    int fbxMaterialCount = fbxScene->GetMaterialCount();

    if (fbxMaterialCount > 0)
    {
        for (int fbxMaterialIndex = 0; fbxMaterialIndex < fbxMaterialCount; ++fbxMaterialIndex)
        {
            FbxSurfaceMaterial* fbxSurfaceMaterial = fbxScene->GetMaterial(fbxMaterialIndex);

            loadMaterial(dirname, fbxSurfaceMaterial);
        }
    }
    else
    {
        Material material;
        material.name = "Dummy";
        material.diffuseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        m_model.materials.emplace_back(material);
    }
}

void FbxLoad::loadMaterial(const char* dirname, FbxSurfaceMaterial* fbxSurfaceMaterial)
{
    Material material;

    material.name = fbxSurfaceMaterial->GetName();

    // ディフューズカラー
    FbxProperty fbxDiffuseProperty = fbxSurfaceMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
    FbxProperty fbxDiffuseFactorProperty = fbxSurfaceMaterial->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);
    if (fbxDiffuseProperty.IsValid() && fbxDiffuseFactorProperty.IsValid())
    {
        FbxDouble fbxFactor = fbxDiffuseFactorProperty.Get<FbxDouble>();
        FbxDouble3 fbxColor = fbxDiffuseProperty.Get<FbxDouble3>();

        material.diffuseColor.x = static_cast<float>(fbxColor[0] * fbxFactor);
        material.diffuseColor.y = static_cast<float>(fbxColor[1] * fbxFactor);
        material.diffuseColor.z = static_cast<float>(fbxColor[2] * fbxFactor);
        material.diffuseColor.w = 1.0f;
    }

    // ディフューズテクスチャ
    if (fbxDiffuseProperty.IsValid())
    {
        int fbxTextureCount = fbxDiffuseProperty.GetSrcObjectCount<FbxFileTexture>();
        if (fbxTextureCount > 0)
        {
            FbxFileTexture* fbxTexture = fbxDiffuseProperty.GetSrcObject<FbxFileTexture>();

            const char* relativeFileName = fbxTexture->GetRelativeFileName();

            // テクスチャファイルが存在しているか確認
            char filename[256];
            ::_makepath_s(filename, sizeof(filename), nullptr, dirname, relativeFileName, nullptr);
            std::ifstream istream(filename, std::ios::binary);
            if (istream.is_open())
            {
                // ディレクトリパスを結合した完全パスを設定する
                material.textureName[0] = filename;
            }
            else
            {
                // 見つからなかった場合はモデルと同ディレクトリにあれば読み込めるようにする
                char fname[256], ext[32];
                ::_splitpath_s(relativeFileName, nullptr, 0, nullptr, 0, fname, sizeof(fname), ext, sizeof(ext));
                char fallback[256];
                ::_makepath_s(fallback, sizeof(fallback), nullptr, dirname, fname, ext);
                material.textureName[0] = fallback;
            }
        }
    }

    m_model.materials.emplace_back(material);
}

void FbxLoad::loadAnimations(FbxScene* fbxScene, const char* name, bool append)
{
    // すべてのアニメーション名を取得
    FbxArray<FbxString*> fbxAnimStackNames;
    fbxScene->FillAnimStackNameArray(fbxAnimStackNames);

    int fbxAnimationCount = fbxAnimStackNames.Size();
    for (int fbxAnimationIndex = 0; fbxAnimationIndex < fbxAnimationCount; ++fbxAnimationIndex)
    {
        m_model.animations.emplace_back(Animation());
        Animation& animation = m_model.animations.back();

        // アニメーションデータのサンプリング設定
        FbxTime::EMode fbxTimeMode = fbxScene->GetGlobalSettings().GetTimeMode();
        FbxTime fbxFrameTime;
        fbxFrameTime.SetTime(0, 0, 0, 1, 0, fbxTimeMode);

        float samplingRate = static_cast<float>(fbxFrameTime.GetFrameRate(fbxTimeMode));
        float samplingTime = 1.0f / samplingRate;

        FbxString* fbxAnimStackName = fbxAnimStackNames.GetAt(fbxAnimationIndex);
        FbxAnimStack* fbxAnimStack = fbxScene->FindMember<FbxAnimStack>(fbxAnimStackName->Buffer());

        // 再生するアニメーションを指定する。
        fbxScene->SetCurrentAnimationStack(fbxAnimStack);

        // アニメーションの再生開始時間と再生終了時間を取得する
        FbxTakeInfo* fbxTakeInfo = fbxScene->GetTakeInfo(fbxAnimStackName->Buffer());
        FbxTime fbxStartTime = fbxTakeInfo->mLocalTimeSpan.GetStart();
        FbxTime fbxEndTime = fbxTakeInfo->mLocalTimeSpan.GetStop();

        // 抽出するデータは60フレーム基準でサンプリングする
        FbxTime fbxSamplingStep;
        fbxSamplingStep.SetTime(0, 0, 1, 0, 0, fbxTimeMode);
        fbxSamplingStep = static_cast<FbxLongLong>(fbxSamplingStep.Get() * samplingTime);

        int frameCount = static_cast<int>((fbxEndTime.Get() - fbxStartTime.Get()) / fbxSamplingStep.Get());

        // アニメーションの対象となるノードを列挙する
        std::vector<FbxNode*> fbxNodes;
        if (append)
        {
            // ノード名を比較して対象ノードを列挙する
            // ※名前が重複していると失敗する場合がある
            FbxNode* fbxRootNode = fbxScene->GetRootNode();
            for (Bone& node : m_model.bones)
            {
                FbxNode* fbxNode = fbxRootNode->FindChild(node.name.c_str(), true, true);
                fbxNodes.emplace_back(fbxNode);
            }
        }
        else
        {
            // ノードの完全パスを比較して対象ノードを列挙する（重い）
            // ※必ずモデルとアニメーションのFBXのノードツリー構成が一致している必要がある
            for (Bone& node : m_model.bones)
            {
                FbxNode* fbxAnimationNode = nullptr;
                for (int fbxNodeIndex = 0; fbxNodeIndex < fbxScene->GetNodeCount(); ++fbxNodeIndex)
                {
                    FbxNode* fbxNode = fbxScene->GetNode(fbxNodeIndex);
                    UINT64 nodeId = getNodeId(fbxNode);
                    if (node.id == nodeId)
                    {
                        fbxAnimationNode = fbxNode;
                        break;
                    }
                }
                fbxNodes.emplace_back(fbxAnimationNode);
            }
        }

        // アニメーション名
        char animationName[256];
        if (name != nullptr && fbxAnimationCount == 1)
        {
            ::sprintf_s(animationName, "%s", name);
        }
        else
        {
            ::sprintf_s(animationName, "%s", fbxAnimStackName->Buffer());
        }

        animation.name = animationName;

        // アニメーションデータを抽出する
        animation.secondsLength = frameCount * samplingTime;
        animation.keyframes.resize(frameCount + 1);

        float seconds = 0.0f;
        Keyframe* keyframe = animation.keyframes.data();
        size_t fbxNodeCount = fbxNodes.size();
        for (FbxTime fbxCurrentTime = fbxStartTime; fbxCurrentTime < fbxEndTime; fbxCurrentTime += fbxSamplingStep, ++keyframe)
        {
            // キーフレーム毎の姿勢データを取り出す。
            keyframe->seconds = seconds;
            keyframe->nodeKeys.resize(fbxNodeCount);
            for (size_t fbxNodeIndex = 0; fbxNodeIndex < fbxNodeCount; ++fbxNodeIndex)
            {
                NodeKeyData& keyData = keyframe->nodeKeys.at(fbxNodeIndex);
                FbxNode* fbxNode = fbxNodes.at(fbxNodeIndex);
                if (fbxNode == nullptr)
                {
                    // アニメーション対象のノードがなかったのでダミーデータを設定
                    Bone& node = m_model.bones.at(fbxNodeIndex);
                    keyData.scale = node.scale;
                    keyData.rotate = node.rotate;
                    keyData.translate = node.translate;
                }
                else if (fbxNodeIndex == rootMotionNodeIndex)
                {
                    // ルートモーションは無視する
                    keyData.scale = DirectX::XMFLOAT3(1, 1, 1);
                    keyData.rotate = DirectX::XMFLOAT4(0, 0, 0, 1);
                    keyData.translate = DirectX::XMFLOAT3(0, 0, 0);
                }
                else
                {
                    // 指定時間のローカル行列からスケール値、回転値、移動値を取り出す。
                    const FbxAMatrix& fbxLocalTransform = fbxNode->EvaluateLocalTransform(fbxCurrentTime);

                    keyData.scale = FbxDouble4ToFloat3(fbxLocalTransform.GetS());
                    keyData.rotate = FbxDouble4ToFloat4(fbxLocalTransform.GetQ());
                    keyData.translate = FbxDouble4ToFloat3(fbxLocalTransform.GetT());
                }
                // 座標系変換
                convertTranslationFromRHtoLH(keyData.translate);
                convertRotationFromRHtoLH(keyData.rotate);
            }
            seconds += samplingTime;
        }
    }
}

void FbxLoad::convertTranslationFromRHtoLH(Vector3& translate)
{
    translate.x = -translate.x;
}

void FbxLoad::convertRotationFromRHtoLH(Vector4& rotate)
{
    rotate.x = -rotate.x;
    rotate.w = -rotate.w;
}

void FbxLoad::convertMatrixFromRHtoLH(Matrix& matrix)
{
    matrix._12 = -matrix._12;
    matrix._13 = -matrix._13;
    matrix._21 = -matrix._21;
    matrix._31 = -matrix._31;
    matrix._41 = -matrix._41;
}

void FbxLoad::convertIndexBufferFromRHtoLH(std::vector<UINT>& indices)
{
    size_t size = indices.size();
    UINT* p = indices.data();
    for (size_t i = 0; i < size; i += 3)
    {
        UINT temp = p[1];
        p[1] = p[2];
        p[2] = temp;

        p += 3;
    }
}

std::string FbxLoad::getNodePath(FbxNode* fbxNode) const
{
    std::string parentNodeName;

    FbxNode* fbxParentNode = fbxNode->GetParent();
    if (fbxParentNode != nullptr)
    {
        parentNodeName = getNodePath(fbxParentNode);
        return parentNodeName + "/" + fbxNode->GetName();
    }

    return fbxNode->GetName();
}

UINT64 FbxLoad::getNodeId(FbxNode* fbxNode)
{
    return reinterpret_cast<UINT64>(fbxNode);
}

int FbxLoad::findNodeIndex(UINT64 nodeId) const
{
    int nodeCount = static_cast<int>(m_model.bones.size());
    for (int i = 0; i < nodeCount; ++i)
    {
        if (m_model.bones[i].id == nodeId)
        {
            return i;
        }
    }
    return -1;
}

int FbxLoad::findMaterialIndex(FbxScene* fbxScene, const FbxSurfaceMaterial* fbxSurfaceMaterial)
{
    int fbxMaterialCount = fbxScene->GetMaterialCount();

    for (int i = 0; i < fbxMaterialCount; ++i)
    {
        if (fbxScene->GetMaterial(i) == fbxSurfaceMaterial)
        {
            return i;
        }
    }
    return -1;
}