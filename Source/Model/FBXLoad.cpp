#include "pch.h"
#include "FbxLoad.h"

static Matrix ToMatrix(const FbxAMatrix& m)
{
    Matrix result;

    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            result.m[r][c] = (float)m[r][c];

    return result;
}

static Vector3 ToVector3(const FbxVector4& v)
{
    return { (float)v[0], (float)v[1], (float)v[2] };
}

static Vector2 ToVector2(const FbxVector2& v)
{
    return { (float)v[0], (float)v[1] };
}

static Quaternion ToQuaternion(const FbxQuaternion& q)
{
    return { (float)q[0], (float)q[1], (float)q[2], (float)q[3] };
}

static Vector3 ReadNormal(FbxLayerElementNormal* elem, int ctrlIndex, int vertexId)
{
    if (!elem) return { 0, 1, 0 };

    int index = 0;

    switch (elem->GetMappingMode())
    {
    case FbxLayerElement::eByControlPoint:
        index = (elem->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
            ? elem->GetIndexArray().GetAt(ctrlIndex)
            : ctrlIndex;
        break;

    case FbxLayerElement::eByPolygonVertex:
        index = (elem->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
            ? elem->GetIndexArray().GetAt(vertexId)
            : vertexId;
        break;

    default:
        return { 0, 1, 0 };
    }

    return ToVector3(elem->GetDirectArray().GetAt(index));
}

static Vector4 ReadTangent(FbxLayerElementTangent* elem, int ctrlIndex, int vertexId)
{
    if (!elem) return { 0, 0, 0, 1 };

    int index = 0;

    switch (elem->GetMappingMode())
    {
    case FbxLayerElement::eByControlPoint:
        index = (elem->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
            ? elem->GetIndexArray().GetAt(ctrlIndex)
            : ctrlIndex;
        break;

    case FbxLayerElement::eByPolygonVertex:
        index = (elem->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
            ? elem->GetIndexArray().GetAt(vertexId)
            : vertexId;
        break;

    default:
        return { 0, 0, 0, 1 };
    }

    FbxVector4 t = elem->GetDirectArray().GetAt(index);
    return { (float)t[0], (float)t[1], (float)t[2], 1.0f };
}

FbxLoad::FbxLoad()
{
    initializeFbx();
}

FbxLoad::~FbxLoad()
{
    destroyFbx();
}

FbxLoad::FbxLoad(FbxLoad&& other) noexcept
    : m_model(std::move(other.m_model))
    , m_manager(other.m_manager)
    , m_scene(other.m_scene)
{
    other.m_manager = nullptr;
    other.m_scene = nullptr;
}

FbxLoad& FbxLoad::operator=(FbxLoad&& other) noexcept
{
    if (this != &other)
    {
        destroyFbx();
        m_model = std::move(other.m_model);
        m_manager = other.m_manager;
        m_scene = other.m_scene;
        other.m_manager = nullptr;
        other.m_scene = nullptr;
    }
    return *this;
}

void FbxLoad::initializeFbx()
{
    m_manager = FbxManager::Create();
    assert(m_manager);

    FbxIOSettings* ios = FbxIOSettings::Create(m_manager, IOSROOT);
    m_manager->SetIOSettings(ios);

    m_scene = FbxScene::Create(m_manager, "Scene");
}

void FbxLoad::destroyFbx()
{
    if (m_scene)
    {
        m_scene->Destroy();
        m_scene = nullptr;
    }
    if (m_manager)
    {
        m_manager->Destroy();
        m_manager = nullptr;
    }
}

bool FbxLoad::load(const std::string& path)
{
    FbxImporter* importer = FbxImporter::Create(m_manager, "");

    if (!importer->Initialize(path.c_str(), -1, m_manager->GetIOSettings()))
    {
        importer->Destroy();
        return false;
    }

    if (!importer->Import(m_scene))
    {
        importer->Destroy();
        return false;
    }

    importer->Destroy();

    //! ConvertScene は使わない（ジオメトリに反映されないため）
    //! 代わりに元の座標系から DirectX 左手Y-Up への変換行列を算出し、
    //! bakeNodeTransforms 内でジオメトリに直接適用する

    parseScene();
    return true;
}

void FbxLoad::bakeNodeTransforms(FbxNode* node)
{
    if (!node) return;

    FbxMesh* mesh = node->GetMesh();
    if (mesh)
    {
        FbxAMatrix globalTransform = node->EvaluateGlobalTransform();

        //! ジオメトリオフセット（Geometric Transform）を取得
        FbxVector4 geoT = node->GetGeometricTranslation(FbxNode::eSourcePivot);
        FbxVector4 geoR = node->GetGeometricRotation(FbxNode::eSourcePivot);
        FbxVector4 geoS = node->GetGeometricScaling(FbxNode::eSourcePivot);

        FbxAMatrix geoMat;
        geoMat.SetTRS(geoT, geoR, geoS);

        //! 最終変換 = 座標系補正 × ノードグローバル変換 × ジオメトリオフセット
        FbxAMatrix totalTransform = m_axisFixMatrix * globalTransform * geoMat;

        //! コントロールポイント（頂点座標）を直接変換
        FbxVector4* ctrlPoints = mesh->GetControlPoints();
        int ctrlCount = mesh->GetControlPointsCount();

        for (int i = 0; i < ctrlCount; ++i)
        {
            ctrlPoints[i] = totalTransform.MultT(ctrlPoints[i]);
        }

        //! 法線・タンジェント変換用（回転のみ）
        FbxAMatrix normalTransform = totalTransform;
        normalTransform.SetT(FbxVector4(0, 0, 0, 0));
        normalTransform.SetS(FbxVector4(1, 1, 1, 1));

        for (int layerIdx = 0; layerIdx < mesh->GetLayerCount(); ++layerIdx)
        {
            FbxLayerElementNormal* normalElem = mesh->GetLayer(layerIdx)->GetNormals();
            if (normalElem)
            {
                int normalCount = normalElem->GetDirectArray().GetCount();
                for (int i = 0; i < normalCount; ++i)
                {
                    FbxVector4 n = normalElem->GetDirectArray().GetAt(i);
                    n = normalTransform.MultT(n);
                    n.Normalize();
                    normalElem->GetDirectArray().SetAt(i, n);
                }
            }

            FbxLayerElementTangent* tangentElem = mesh->GetLayer(layerIdx)->GetTangents();
            if (tangentElem)
            {
                int tangentCount = tangentElem->GetDirectArray().GetCount();
                for (int i = 0; i < tangentCount; ++i)
                {
                    FbxVector4 t = tangentElem->GetDirectArray().GetAt(i);
                    t = normalTransform.MultT(t);
                    t.Normalize();
                    tangentElem->GetDirectArray().SetAt(i, t);
                }
            }
        }
    }

    //! 子ノードも再帰処理
    for (int i = 0; i < node->GetChildCount(); ++i)
    {
        bakeNodeTransforms(node->GetChild(i));
    }
}

void FbxLoad::parseScene()
{
    m_model = Model();

    //! 三角形化
    FbxGeometryConverter converter(m_manager);
    converter.Triangulate(m_scene, true);

    //! マテリアル別メッシュ分割
    converter.SplitMeshesPerMaterial(m_scene, true);

    //! 座標系補正行列を算出（元の座標系 → DirectX 左手Y-Up）
    m_axisFixMatrix = computeAxisFixMatrix();

    //! 単位スケールを補正行列に組み込む
    //! GetScaleFactor() = 1シーン単位が何cmか
    //!   例: Maya/Blender(m) → 100.0, 3dsMax(inch) → 2.54, cm → 1.0
    //! PMXは概ね 1単位≒8cm（身長160cm ≒ 20単位）
    double sceneFactor = m_scene->GetGlobalSettings().GetSystemUnit().GetScaleFactor();
    double unitScale = sceneFactor * 0.125; //!< シーン単位→cm→PMX互換

    FbxAMatrix scaleMat;
    scaleMat.SetIdentity();
    scaleMat.SetS(FbxVector4(unitScale, unitScale, unitScale));

    m_axisFixMatrix = scaleMat * m_axisFixMatrix;

    //! Triangulate / Split の後にベイク（順序が重要）
    bakeNodeTransforms(m_scene->GetRootNode());

    FbxNode* root = m_scene->GetRootNode();
    if (!root) return;

    parseMaterials();

    //! 先にスケルトンを全て構築する（再帰探索）
    for (int i = 0; i < root->GetChildCount(); ++i)
        parseNode(root->GetChild(i), -1);

    parseAnimations();
}

void FbxLoad::parseNode(FbxNode* node, int parentBoneIndex)
{
    if (!node) return;

    int boneIndex = parentBoneIndex;

    if (node->GetNodeAttribute())
    {
        auto type = node->GetNodeAttribute()->GetAttributeType();

        if (type == FbxNodeAttribute::eSkeleton)
        {
            parseSkeleton(node, parentBoneIndex);

            auto it = m_model.skeleton.boneMap.find(node->GetName());
            if (it != m_model.skeleton.boneMap.end())
                boneIndex = it->second;
        }
        else if (type == FbxNodeAttribute::eMesh)
        {
            parseMesh(node);
        }
    }

    //! boneMap から現在ノードのインデックスを取得（メッシュノードの場合でも子に伝搬）
    auto it = m_model.skeleton.boneMap.find(node->GetName());
    if (it != m_model.skeleton.boneMap.end())
        boneIndex = it->second;

    for (int i = 0; i < node->GetChildCount(); ++i)
        parseNode(node->GetChild(i), boneIndex);
}

void FbxLoad::parseMesh(FbxNode* node)
{
    FbxMesh* mesh = node->GetMesh();
    if (!mesh) return;

    Mesh dst;
    dst.name = node->GetName();
    dst.bindTransform = Matrix::Identity;

    FbxVector4* ctrlPoints = mesh->GetControlPoints();
    int polyCount = mesh->GetPolygonCount();

    FbxLayerElementNormal* normalElem = mesh->GetElementNormal();
    FbxLayerElementUV* uvElem = mesh->GetElementUV();
    FbxLayerElementTangent* tanElem = mesh->GetElementTangent();
    FbxLayerElementMaterial* materialElem = mesh->GetElementMaterial();

    //! ノードのローカルマテリアルインデックス → シーングローバルインデックス変換テーブル
    std::vector<int> localToGlobalMaterial;
    int nodeMaterialCount = node->GetMaterialCount();
    localToGlobalMaterial.reserve(nodeMaterialCount);
    for (int mi = 0; mi < nodeMaterialCount; ++mi)
    {
        FbxSurfaceMaterial* nodeMat = node->GetMaterial(mi);
        int globalIdx = 0; //!< フォールバック

        if (nodeMat)
        {
            //! シーン全体のマテリアルリストからグローバルインデックスを検索
            int sceneMatCount = m_scene->GetMaterialCount();
            for (int si = 0; si < sceneMatCount; ++si)
            {
                if (m_scene->GetMaterial(si) == nodeMat)
                {
                    globalIdx = si;
                    break;
                }
            }
        }

        localToGlobalMaterial.push_back(globalIdx);
    }

    //! マテリアル別にインデックスを分類
    //! key = materialIndex, value = 三角形ごとのインデックスリスト
    std::map<int, std::vector<uint32_t>> subMeshIndices;

    //! コントロールポイント → 展開後頂点のマッピング（スキニング用）
    std::unordered_map<int, std::vector<uint32_t>> ctrlToVertex;

    for (int i = 0; i < polyCount; ++i)
    {
        int localMaterialIndex = 0;
        if (materialElem)
            localMaterialIndex = materialElem->GetIndexArray().GetAt(i);

        //! ローカルインデックスをグローバルインデックスに変換
        int materialIndex = 0;
        if (localMaterialIndex >= 0 && localMaterialIndex < (int)localToGlobalMaterial.size())
            materialIndex = localToGlobalMaterial[localMaterialIndex];
        else if (!localToGlobalMaterial.empty())
            materialIndex = localToGlobalMaterial[0];

        for (int j = 0; j < 3; ++j)
        {
            int ctrlIndex = mesh->GetPolygonVertex(i, j);
            int vertexId = i * 3 + j;

            Vertex v;
            v.position = ToVector3(ctrlPoints[ctrlIndex]);

            //! Normal
            v.normal = ReadNormal(normalElem, ctrlIndex, vertexId);

            //! UV
            if (uvElem)
            {
                FbxVector2 uv;
                bool unmapped = false;
                mesh->GetPolygonVertexUV(i, j, uvElem->GetName(), uv, unmapped);
                v.uv = { (float)uv[0], 1.0f - (float)uv[1] };
            }

            //! Tangent
            v.tangent = ReadTangent(tanElem, ctrlIndex, vertexId);

            uint32_t newIndex = (uint32_t)dst.vertices.size();
            dst.vertices.push_back(v);
            dst.indices.push_back(newIndex);

            subMeshIndices[materialIndex].push_back(newIndex);

            //! コントロールポイント → 展開後頂点のマッピングを記録
            ctrlToVertex[ctrlIndex].push_back(newIndex);
        }
    }

    //! サブメッシュ構築（マテリアル順にインデックスを再配置）
    {
        std::vector<uint32_t> sortedIndices;
        sortedIndices.reserve(dst.indices.size());

        for (auto& [matIdx, idxList] : subMeshIndices)
        {
            SubMesh sub;
            sub.startIndex = (uint32_t)sortedIndices.size();
            sub.indexCount = (uint32_t)idxList.size();
            sub.materialIndex = (uint32_t)matIdx;
            dst.subMeshes.push_back(sub);

            sortedIndices.insert(sortedIndices.end(), idxList.begin(), idxList.end());
        }

        dst.indices = std::move(sortedIndices);
    }

    //! スキニング情報の抽出
    extractSkinning(mesh, dst, ctrlToVertex);

    //! タンジェントがFBXに存在しなかった場合は自動生成
    if (!tanElem)
        generateTangents(dst);

    m_model.meshes.push_back(std::move(dst));
}

void FbxLoad::parseSkeleton(FbxNode* node, int parentIndex)
{
    //! 既に登録済みならスキップ
    if (m_model.skeleton.boneMap.contains(node->GetName()))
        return;

    Skeleton::Bone bone;
    bone.name = node->GetName();
    bone.parentIndex = parentIndex;

    FbxAMatrix globalBind = node->EvaluateGlobalTransform();
    bone.inverseBindMatrix = ToMatrix(globalBind.Inverse());

    int index = (int)m_model.skeleton.bones.size();
    m_model.skeleton.bones.push_back(bone);
    m_model.skeleton.boneMap[bone.name] = index;
}

void FbxLoad::parseMaterials()
{
    int materialCount = m_scene->GetMaterialCount();
    m_model.materials.reserve(materialCount);

    for (int i = 0; i < materialCount; ++i)
    {
        FbxSurfaceMaterial* fbxMat = m_scene->GetMaterial(i);
        if (!fbxMat) continue;

        Material mat;
        mat.name = fbxMat->GetName();

        //! --- Diffuse / BaseColor テクスチャ ---
        FbxProperty propDiffuse = fbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse);
        if (!propDiffuse.IsValid())
            propDiffuse = fbxMat->FindProperty("BaseColor");

        if (propDiffuse.IsValid())
        {
            //! テクスチャ
            int texCount = propDiffuse.GetSrcObjectCount<FbxFileTexture>();
            if (texCount > 0)
            {
                FbxFileTexture* tex = propDiffuse.GetSrcObject<FbxFileTexture>(0);
                if (tex)
                    mat.texturePath = toRelativePath(tex->GetFileName());
            }

            //! Diffuse カラー
            if (propDiffuse.GetPropertyDataType().GetType() == eFbxDouble3)
            {
                FbxDouble3 c = propDiffuse.Get<FbxDouble3>();
                mat.diffuseColor = { (float)c[0], (float)c[1], (float)c[2], 1.0f };
            }
        }

        //! --- Normal Map テクスチャ ---
        FbxProperty propNormal = fbxMat->FindProperty(FbxSurfaceMaterial::sNormalMap);
        if (propNormal.IsValid())
        {
            int texCount = propNormal.GetSrcObjectCount<FbxFileTexture>();
            if (texCount > 0)
            {
                FbxFileTexture* tex = propNormal.GetSrcObject<FbxFileTexture>(0);
                if (tex)
                    mat.normalMapPath = toRelativePath(tex->GetFileName());
            }
        }

        //! --- Specular ---
        FbxProperty propSpec = fbxMat->FindProperty(FbxSurfaceMaterial::sSpecular);
        if (propSpec.IsValid() && propSpec.GetPropertyDataType().GetType() == eFbxDouble3)
        {
            FbxDouble3 c = propSpec.Get<FbxDouble3>();
            mat.specularColor = { (float)c[0], (float)c[1], (float)c[2] };
        }

        FbxProperty propShininess = fbxMat->FindProperty(FbxSurfaceMaterial::sShininess);
        if (propShininess.IsValid())
            mat.specularPower = (float)propShininess.Get<FbxDouble>();

        //! --- Ambient ---
        FbxProperty propAmbient = fbxMat->FindProperty(FbxSurfaceMaterial::sAmbient);
        if (propAmbient.IsValid() && propAmbient.GetPropertyDataType().GetType() == eFbxDouble3)
        {
            FbxDouble3 c = propAmbient.Get<FbxDouble3>();
            mat.ambientColor = { (float)c[0], (float)c[1], (float)c[2] };
        }

        //! --- Emissive ---
        FbxProperty propEmissive = fbxMat->FindProperty(FbxSurfaceMaterial::sEmissive);
        if (propEmissive.IsValid() && propEmissive.GetPropertyDataType().GetType() == eFbxDouble3)
        {
            FbxDouble3 c = propEmissive.Get<FbxDouble3>();
            mat.emissiveColor = { (float)c[0], (float)c[1], (float)c[2] };
        }

        m_model.materials.push_back(std::move(mat));
    }
}

void FbxLoad::parseAnimations()
{
    if (m_model.skeleton.empty()) return;

    int stackCount = m_scene->GetSrcObjectCount<FbxAnimStack>();

    for (int s = 0; s < stackCount; ++s)
    {
        FbxAnimStack* stack = m_scene->GetSrcObject<FbxAnimStack>(s);
        m_scene->SetCurrentAnimationStack(stack);

        AnimationClip clip;
        clip.name = stack->GetName();

        FbxTakeInfo* take = m_scene->GetTakeInfo(stack->GetName());
        if (!take) continue;

        FbxTime start = take->mLocalTimeSpan.GetStart();
        FbxTime end = take->mLocalTimeSpan.GetStop();

        clip.duration = (float)(end.GetSecondDouble() - start.GetSecondDouble());
        clip.ticksPerSecond = 30.0f;

        size_t boneCount = m_model.skeleton.bones.size();
        clip.boneAnimations.resize(boneCount);

        //! ノードキャッシュ（毎フレーム FindNodeByName を呼ばない）
        std::vector<FbxNode*> boneNodes(boneCount, nullptr);
        for (size_t b = 0; b < boneCount; ++b)
        {
            boneNodes[b] = m_scene->FindNodeByName(
                m_model.skeleton.bones[b].name.c_str());
        }

        FbxTime time = start;
        FbxTime frameStep = FbxTime::GetOneFrameValue(FbxTime::eFrames30);

        while (time <= end)
        {
            float t = (float)(time.GetSecondDouble() - start.GetSecondDouble());

            for (size_t b = 0; b < boneCount; ++b)
            {
                FbxNode* node = boneNodes[b];
                if (!node) continue;

                FbxAMatrix mat = node->EvaluateLocalTransform(time);

                Keyframe kf;
                kf.time = t;
                kf.transform.translation = ToVector3(mat.GetT());
                kf.transform.rotation = ToQuaternion(mat.GetQ());
                kf.transform.scale = ToVector3(mat.GetS());

                clip.boneAnimations[b].keyframes.push_back(kf);
            }

            time += frameStep;
        }

        m_model.animations.push_back(std::move(clip));
    }
}

void FbxLoad::extractSkinning(FbxMesh* mesh, Mesh& dstMesh, const std::unordered_map<int, std::vector<uint32_t>>& ctrlToVertex)
{
    int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
    if (skinCount == 0) return;

    struct WeightData
    {
        int bone;
        float weight;
    };

    //! 展開済み頂点ごとのウェイトリスト
    std::vector<std::vector<WeightData>> weightTable(dstMesh.vertices.size());

    for (int i = 0; i < skinCount; ++i)
    {
        FbxSkin* skin = static_cast<FbxSkin*>(
            mesh->GetDeformer(i, FbxDeformer::eSkin));

        int clusterCount = skin->GetClusterCount();

        for (int j = 0; j < clusterCount; ++j)
        {
            FbxCluster* cluster = skin->GetCluster(j);
            if (!cluster->GetLink()) continue;

            std::string boneName = cluster->GetLink()->GetName();
            int boneIndex = m_model.skeleton.findBone(boneName);
            if (boneIndex < 0) continue;

            //! Inverse Bind Pose を Cluster から取得して上書き
            FbxAMatrix meshBindPose, clusterBindPose;
            cluster->GetTransformMatrix(meshBindPose);
            cluster->GetTransformLinkMatrix(clusterBindPose);
            FbxAMatrix invBindPose = clusterBindPose.Inverse() * meshBindPose;
            m_model.skeleton.bones[boneIndex].inverseBindMatrix = ToMatrix(invBindPose);

            int* indices = cluster->GetControlPointIndices();
            double* weights = cluster->GetControlPointWeights();
            int count = cluster->GetControlPointIndicesCount();

            for (int k = 0; k < count; ++k)
            {
                int ctrlIndex = indices[k];
                float w = (float)weights[k];

                if (w <= 0.0f) continue;

                //! コントロールポイントに対応する全展開済み頂点にウェイトを配分
                auto it = ctrlToVertex.find(ctrlIndex);
                if (it == ctrlToVertex.end()) continue;

                for (uint32_t vertIdx : it->second)
                {
                    weightTable[vertIdx].push_back({ boneIndex, w });
                }
            }
        }
    }

    //! 上位4本に制限して正規化
    for (size_t i = 0; i < dstMesh.vertices.size(); ++i)
    {
        auto& weights = weightTable[i];

        //! ウェイト降順ソート
        std::sort(weights.begin(), weights.end(),
            [](const WeightData& a, const WeightData& b)
            { return a.weight > b.weight; });

        float total = 0.0f;
        int limit = std::min((int)weights.size(), 4);

        for (int j = 0; j < limit; ++j)
        {
            dstMesh.vertices[i].boneIndices[j] = (uint32_t)weights[j].bone;
            dstMesh.vertices[i].boneWeights[j] = weights[j].weight;
            total += weights[j].weight;
        }

        //! 正規化
        if (total > 0.0f)
        {
            float invTotal = 1.0f / total;
            for (int j = 0; j < limit; j++)
                dstMesh.vertices[i].boneWeights[j] *= invTotal;
        }
    }
}

void FbxLoad::generateTangents(Mesh& dstMesh)
{
    size_t vertCount = dstMesh.vertices.size();
    if (vertCount == 0) return;

    std::vector<Vector3> tan1(vertCount, Vector3(0, 0, 0));
    std::vector<Vector3> tan2(vertCount, Vector3(0, 0, 0));

    size_t indexCount = dstMesh.indices.size();

    for (size_t i = 0; i + 2 < indexCount; i += 3)
    {
        uint32_t i0 = dstMesh.indices[i + 0];
        uint32_t i1 = dstMesh.indices[i + 1];
        uint32_t i2 = dstMesh.indices[i + 2];

        const Vertex& v0 = dstMesh.vertices[i0];
        const Vertex& v1 = dstMesh.vertices[i1];
        const Vertex& v2 = dstMesh.vertices[i2];

        Vector3 edge1 = v1.position - v0.position;
        Vector3 edge2 = v2.position - v0.position;

        float du1 = v1.uv.x - v0.uv.x;
        float dv1 = v1.uv.y - v0.uv.y;
        float du2 = v2.uv.x - v0.uv.x;
        float dv2 = v2.uv.y - v0.uv.y;

        float det = du1 * dv2 - du2 * dv1;
        if (std::abs(det) < 1e-8f) continue;

        float invDet = 1.0f / det;

        Vector3 sdir = (edge1 * dv2 - edge2 * dv1) * invDet;
        Vector3 tdir = (edge2 * du1 - edge1 * du2) * invDet;

        tan1[i0] += sdir; tan1[i1] += sdir; tan1[i2] += sdir;
        tan2[i0] += tdir; tan2[i1] += tdir; tan2[i2] += tdir;
    }

    for (size_t i = 0; i < vertCount; ++i)
    {
        const Vector3& n = dstMesh.vertices[i].normal;
        const Vector3& t = tan1[i];

        //! Gram-Schmidt 直交化
        Vector3 tangent = t - n * n.Dot(t);
        tangent.Normalize();

        //! Handedness
        float w = (n.Cross(t).Dot(tan2[i]) < 0.0f) ? -1.0f : 1.0f;

        dstMesh.vertices[i].tangent = { tangent.x, tangent.y, tangent.z, w };
    }
}

std::unordered_map<int, std::vector<uint32_t>> FbxLoad::buildCtrlToVertexMap(FbxMesh* mesh) const
{
    std::unordered_map<int, std::vector<uint32_t>> result;

    int polyCount = mesh->GetPolygonCount();
    uint32_t vertexId = 0;

    for (int i = 0; i < polyCount; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            int ctrlIndex = mesh->GetPolygonVertex(i, j);
            result[ctrlIndex].push_back(vertexId);
            ++vertexId;
        }
    }

    return result;
}

FbxAMatrix FbxLoad::computeAxisFixMatrix() const
{
    FbxAxisSystem sceneAxis = m_scene->GetGlobalSettings().GetAxisSystem();

    //! ターゲット: DirectX 左手系 Y-Up
    FbxAxisSystem targetAxis(FbxAxisSystem::eYAxis,
        FbxAxisSystem::eParityOdd,
        FbxAxisSystem::eLeftHanded);

    //! 同じなら補正不要
    if (sceneAxis == targetAxis)
    {
        FbxAMatrix identity;
        identity.SetIdentity();
        return identity;
    }

    //! シーン座標系の情報を取得
    int upSign = 0;
    FbxAxisSystem::EUpVector upVec = sceneAxis.GetUpVector(upSign);

    int frontSign = 0;
    FbxAxisSystem::EFrontVector frontVec = sceneAxis.GetFrontVector(frontSign);

    FbxAxisSystem::ECoordSystem coordSys = sceneAxis.GetCoorSystem();

    //! ソース座標系の Up 軸ベクトル
    FbxVector4 srcUp(0, 0, 0, 0);
    if (upVec == FbxAxisSystem::eXAxis) srcUp[0] = upSign;
    else if (upVec == FbxAxisSystem::eYAxis) srcUp[1] = upSign;
    else if (upVec == FbxAxisSystem::eZAxis) srcUp[2] = upSign;

    //! ソース座標系の Front 軸ベクトル
    FbxVector4 srcFront(0, 0, 0, 0);
    if (upVec == FbxAxisSystem::eXAxis)
    {
        if (frontVec == FbxAxisSystem::eParityEven)
            srcFront[1] = frontSign;
        else
            srcFront[2] = frontSign;
    }
    else if (upVec == FbxAxisSystem::eYAxis)
    {
        if (frontVec == FbxAxisSystem::eParityEven)
            srcFront[0] = frontSign;
        else
            srcFront[2] = frontSign;
    }
    else
    {
        if (frontVec == FbxAxisSystem::eParityEven)
            srcFront[0] = frontSign;
        else
            srcFront[1] = frontSign;
    }

    //! ソース座標系の Right 軸 = Up × Front（右手系）、左手系なら反転
    FbxVector4 srcRight(
        srcUp[1] * srcFront[2] - srcUp[2] * srcFront[1],
        srcUp[2] * srcFront[0] - srcUp[0] * srcFront[2],
        srcUp[0] * srcFront[1] - srcUp[1] * srcFront[0],
        0
    );

    if (coordSys == FbxAxisSystem::eLeftHanded)
    {
        srcRight[0] = -srcRight[0];
        srcRight[1] = -srcRight[1];
        srcRight[2] = -srcRight[2];
    }

    //! 変換行列を構築
    //! ソースの基底ベクトル → ターゲット（DirectX 左手系）の基底ベクトル
    //!   ターゲット +X = srcRight
    //!   ターゲット +Y = srcUp
    //!   ターゲット +Z = -srcFront（DirectX 左手系の Forward は -Z）
    //!
    //! FbxAMatrix::MultT は列ベクトル v' = M * v として動作するため
    //! 各列にターゲット基底を配置する
    FbxAMatrix fixMat;
    fixMat.SetIdentity();

    //! Column 0 = ターゲット +X（Right）← srcRight がソースのどの方向か
    fixMat[0][0] = srcRight[0]; fixMat[1][0] = srcRight[1]; fixMat[2][0] = srcRight[2];
    //! Column 1 = ターゲット +Y（Up）← srcUp がソースのどの方向か
    fixMat[0][1] = srcUp[0];    fixMat[1][1] = srcUp[1];    fixMat[2][1] = srcUp[2];
    //! Column 2 = ターゲット +Z = -Front
    fixMat[0][2] = -srcFront[0]; fixMat[1][2] = -srcFront[1]; fixMat[2][2] = -srcFront[2];

    return fixMat;
}