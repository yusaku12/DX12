#include "pch.h"
#include "PmxLoad.h"

PmxLoad::PmxLoad(const std::wstring& filePath, PMXFileData& fileData)
{
    //! PMXモデル読み込み
    pmxLoadFile(filePath, fileData);
}

PmxLoad::~PmxLoad()
{
}

bool PmxLoad::getPMXStringUTF16(std::ifstream& _file, std::wstring& output)
{
    int textSize = 0;

    //! 文字列サイズ(バイト)を取得
    _file.read(reinterpret_cast<char*>(&textSize), 4);

    // textSize 分のバッファを動的に確保（UTF-16 = 2byte）
    std::vector<wchar_t> buffer(textSize / 2);

    //! ファイルから読み込み
    _file.read(reinterpret_cast<char*>(buffer.data()), textSize);

    //! wstring に格納
    output.assign(buffer.begin(), buffer.end());

    return true;
}

bool PmxLoad::getPMXStringUTF8(std::ifstream& _file, std::string& output)
{
    int textSize = 0;

    //! 文字列サイズを読込
    _file.read(reinterpret_cast<char*>(&textSize), 4);

    //! textSize 分のバッファを確保（UTF-8 = 1byte）
    std::vector<char> buffer(textSize);

    _file.read(reinterpret_cast<char*>(buffer.data()), textSize);

    //! そのまま string に格納
    output.assign(buffer.begin(), buffer.end());

    return true;
}

bool PmxLoad::pmxLoadFile(const std::wstring& filePath, PMXFileData& fileData)
{
    //! ファイルが無かったら
    if (filePath.empty())
    {
        LOG_ASSERT_NO_JUDGE("failure file path");
        return false;
    }

    //! ファイル読み込み
    std::ifstream pmxFile{ filePath, (std::ios::binary | std::ios::in) };
    if (pmxFile.fail())
    {
        pmxFile.close();
        LOG_ASSERT_NO_JUDGE("failure read file path");
        return false;
    }

    //! PMXのファイルヘッダー情報読み込み
    bool result = readHeader(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readHeader");
        return false;
    }

    //! PMXのモデル情報はモデルの名前とコメントを読み込む
    result = readModelInfo(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readModelInfo");
        return false;
    }

    //! 頂点情報読み込み
    result = readVertex(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readVertex");
        return false;
    }

    //! フェイス情報読み込み
    result = readFace(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readFace");
        return false;
    }

    //! テクスチャ情報読み込み
    result = readTextures(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readTextures");
        return false;
    }

    //! マテリアル読み込み
    result = readMaterial(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readMaterial");
        return false;
    }

    //! ボーン情報読み込み
    result = readBone(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readBone");
        return false;
    }

    //! モーフ情報読み込み
    result = readMorph(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readMorph");
        return false;
    }

    //! 表示パネル読み込み
    result = readDisplayFrame(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readDisplayFrame");
        return false;
    }

    //! 剛体読み込み
    result = readRigidBody(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readDisplayFrame");
        return false;
    }

    //! ジョイント読み込み
    result = readJoint(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readJoint");
        return false;
    }

    //! ソフトボディ読み込み
    result = readSoftBody(fileData, pmxFile);
    if (!result)
    {
        LOG_ASSERT_NO_JUDGE("failure readSoftBody");
        return false;
    }

    return true;
}

bool PmxLoad::readHeader(PMXFileData& data, std::ifstream& file)
{
    //! magic
    if (!file.read(reinterpret_cast<char*>(data.header.magic.data()), data.header.magic.size()))
    {
        LOG_ASSERT_NO_JUDGE("PMX magic read failed");
        return false;
    }

    if (data.header.magic != PMX_MAGIC_NUMBER)
    {
        LOG_ASSERT_NO_JUDGE("Not PMX file");
        return false;
    }

    //! version
    file.read(reinterpret_cast<char*>(&data.header.version), sizeof(float));

    //! data length
    file.read(reinterpret_cast<char*>(&data.header.dataLength), 1);

    //! 可変ヘッダ
    std::vector<uint8_t> headerData(data.header.dataLength);
    file.read(reinterpret_cast<char*>(headerData.data()), data.header.dataLength);

    data.header.textEncoding = headerData[0];
    data.header.addUVNum = headerData[1];
    data.header.vertexIndexSize = headerData[2];
    data.header.textureIndexSize = headerData[3];
    data.header.materialIndexSize = headerData[4];
    data.header.boneIndexSize = headerData[5];
    data.header.morphIndexSize = headerData[6];
    data.header.rigidBodyIndexSize = headerData[7];

    return true;
}

bool PmxLoad::readModelInfo(PMXFileData& data, std::ifstream& file)
{
    getPMXStringUTF16(file, data.modelInfo.modelName);
    getPMXStringUTF16(file, data.modelInfo.comment);
    getPMXStringUTF8(file, data.modelInfo.englishModelName);
    getPMXStringUTF8(file, data.modelInfo.englishComment);

    //!読み込んだモデル名をログ出力
    LOG_INFO("PMX Model Loaded : " + wstringToString(data.modelInfo.modelName));

    return true;
}

bool PmxLoad::readVertex(PMXFileData& data, std::ifstream& file)
{
    uint32_t vertexCount = 0;
    file.read(reinterpret_cast<char*>(&vertexCount), sizeof(uint32_t));
    data.vertices.resize(vertexCount);

    for (auto& vertex : data.vertices)
    {
        //! position / normal / uv
        file.read(reinterpret_cast<char*>(&vertex.position), 12);
        file.read(reinterpret_cast<char*>(&vertex.normal), 12);
        file.read(reinterpret_cast<char*>(&vertex.uv), 8);

        //! additional UV (max 4)
        for (uint8_t i = 0; i < std::min(data.header.addUVNum, uint8_t(4)); ++i)
        {
            file.read(reinterpret_cast<char*>(&vertex.additionalUV[i]), 16);
        }

        //! weight type
        file.read(reinterpret_cast<char*>(&vertex.weightType), 1);

        switch (vertex.weightType)
        {
        case PMXVertexWeight::BDEF1:
            readIndex(file, data.header.boneIndexSize, vertex.boneIndices[0]);
            break;

        case PMXVertexWeight::BDEF2:
            readIndex(file, data.header.boneIndexSize, vertex.boneIndices[0]);
            readIndex(file, data.header.boneIndexSize, vertex.boneIndices[1]);
            file.read(reinterpret_cast<char*>(&vertex.boneWeights[0]), 4);
            break;

        case PMXVertexWeight::BDEF4:
        case PMXVertexWeight::QDEF:
            for (int i = 0; i < 4; ++i)
                readIndex(file, data.header.boneIndexSize, vertex.boneIndices[i]);

            for (int i = 0; i < 4; ++i)
                file.read(reinterpret_cast<char*>(&vertex.boneWeights[i]), 4);
            break;

        case PMXVertexWeight::SDEF:
            readIndex(file, data.header.boneIndexSize, vertex.boneIndices[0]);
            readIndex(file, data.header.boneIndexSize, vertex.boneIndices[1]);
            file.read(reinterpret_cast<char*>(&vertex.boneWeights[0]), 4);
            file.read(reinterpret_cast<char*>(&vertex.sdefC), 12);
            file.read(reinterpret_cast<char*>(&vertex.sdefR0), 12);
            file.read(reinterpret_cast<char*>(&vertex.sdefR1), 12);
            break;
        }

        //! edge
        file.read(reinterpret_cast<char*>(&vertex.edgeMag), 4);

        if (!file)
        {
            LOG_ASSERT_NO_JUDGE("Vertex read failed");
            return false;
        }
    }

    return true;
}

bool PmxLoad::readFace(PMXFileData& data, std::ifstream& file)
{
    uint32_t indexCount = 0;
    file.read(reinterpret_cast<char*>(&indexCount), sizeof(uint32_t));

    if (indexCount % 3 != 0)
        return false;

    const uint32_t faceCount = indexCount / 3;
    data.faces.resize(faceCount);

    for (uint32_t i = 0; i < faceCount; ++i)
    {
        for (int v = 0; v < 3; ++v)
        {
            int index = readIndex(file, data.header.vertexIndexSize);
            if (index < 0)
                return false;

            data.faces[i].vertices[v] = static_cast<uint32_t>(index);
        }
    }

    return true;
}

bool PmxLoad::readTextures(PMXFileData& data, std::ifstream& file)
{
    uint32_t numOfTexture = 0;
    file.read(reinterpret_cast<char*>(&numOfTexture), sizeof(uint32_t));

    data.textures.resize(numOfTexture);

    for (auto& texture : data.textures)
    {
        getPMXStringUTF16(file, texture.textureName);
    }

    return true;
}

bool PmxLoad::readMaterial(PMXFileData& data, std::ifstream& file)
{
    uint32_t numOfMaterial = 0;
    file.read(reinterpret_cast<char*>(&numOfMaterial), sizeof(uint32_t));
    data.materials.resize(numOfMaterial);

    for (auto& mat : data.materials)
    {
        //! name
        getPMXStringUTF16(file, mat.name);
        getPMXStringUTF8(file, mat.englishName);

        //! basic params
        file.read(reinterpret_cast<char*>(&mat.diffuse), 16);
        file.read(reinterpret_cast<char*>(&mat.specular), 12);
        file.read(reinterpret_cast<char*>(&mat.specularPower), 4);
        file.read(reinterpret_cast<char*>(&mat.ambient), 12);

        //! draw flag
        file.read(reinterpret_cast<char*>(&mat.drawMode), 1);

        //! edge
        file.read(reinterpret_cast<char*>(&mat.edgeColor), 16);
        file.read(reinterpret_cast<char*>(&mat.edgeSize), 4);

        //! texture indices
        int textureIndex = readIndex(file, data.header.textureIndexSize);
        int sphereTextureIndex = readIndex(file, data.header.textureIndexSize);

        file.read(reinterpret_cast<char*>(&mat.sphereMode), 1);
        file.read(reinterpret_cast<char*>(&mat.toonMode), 1);

        int toonTextureIndex = -1;

        if (mat.toonMode == PMXToonMode::Separate)
        {
            toonTextureIndex = readIndex(file, data.header.textureIndexSize);
        }
        else if (mat.toonMode == PMXToonMode::Common)
        {
            uint8_t commonIndex;
            file.read(reinterpret_cast<char*>(&commonIndex), 1);
            toonTextureIndex = commonIndex;
        }
        else
        {
            return false;
        }

        //! resolve texture path
        if (textureIndex >= 0 && textureIndex < (int)data.textures.size())
            mat.texturePath = data.textures[textureIndex].textureName;

        if (sphereTextureIndex >= 0 && sphereTextureIndex < (int)data.textures.size())
            mat.sphereTexturePath = data.textures[sphereTextureIndex].textureName;

        if (toonTextureIndex >= 0 && toonTextureIndex < (int)data.textures.size())
            mat.toonTexturePath = data.textures[toonTextureIndex].textureName;

        //! memo
        getPMXStringUTF16(file, mat.memo);

        //! face vertex count
        file.read(reinterpret_cast<char*>(&mat.numFaceVertices), 4);

        if (!file)
            return false;
    }

    return true;
}

bool PmxLoad::readBone(PMXFileData& data, std::ifstream& file)
{
    uint32_t numOfBone = 0;
    file.read(reinterpret_cast<char*>(&numOfBone), sizeof(uint32_t));

    data.bones.resize(numOfBone);

    for (auto& bone : data.bones)
    {
        //! name
        getPMXStringUTF16(file, bone.name);
        getPMXStringUTF8(file, bone.englishName);

        //! base
        file.read(reinterpret_cast<char*>(&bone.position), 12);
        bone.parentBoneIndex = readIndex(file, data.header.boneIndexSize);
        file.read(reinterpret_cast<char*>(&bone.deformDepth), 4);
        file.read(reinterpret_cast<char*>(&bone.boneFlag), 2);

        //! 表示先
        if (!hasBoneFlag(bone.boneFlag, PMXBoneFlags::TargetShowMode))
        {
            file.read(reinterpret_cast<char*>(&bone.positionOffset), 12);
        }
        else
        {
            bone.linkBoneIndex = readIndex(file, data.header.boneIndexSize);
        }

        //! 付与
        if (hasBoneFlag(bone.boneFlag, PMXBoneFlags::AppendRotate) ||
            hasBoneFlag(bone.boneFlag, PMXBoneFlags::AppendTranslate))
        {
            bone.appendBoneIndex = readIndex(file, data.header.boneIndexSize);
            file.read(reinterpret_cast<char*>(&bone.appendWeight), 4);
        }

        //! 固定軸
        if (hasBoneFlag(bone.boneFlag, PMXBoneFlags::FixedAxis))
        {
            file.read(reinterpret_cast<char*>(&bone.fixedAxis), 12);
        }

        //! ローカル軸
        if (hasBoneFlag(bone.boneFlag, PMXBoneFlags::LocalAxis))
        {
            file.read(reinterpret_cast<char*>(&bone.localXAxis), 12);
            file.read(reinterpret_cast<char*>(&bone.localZAxis), 12);
        }

        //! 外部親変形
        if (hasBoneFlag(bone.boneFlag, PMXBoneFlags::DeformOuterParent))
        {
            file.read(reinterpret_cast<char*>(&bone.keyValue), 4);
        }

        //! IK
        if (hasBoneFlag(bone.boneFlag, PMXBoneFlags::IK))
        {
            bone.ikTargetBoneIndex = readIndex(file, data.header.boneIndexSize);
            file.read(reinterpret_cast<char*>(&bone.ikIterationCount), 4);
            file.read(reinterpret_cast<char*>(&bone.ikLimit), 4);

            uint32_t linkCount = 0;
            file.read(reinterpret_cast<char*>(&linkCount), 4);

            bone.ikLinks.resize(linkCount);
            for (auto& ikLink : bone.ikLinks)
            {
                ikLink.ikBoneIndex = readIndex(file, data.header.boneIndexSize);
                file.read(reinterpret_cast<char*>(&ikLink.enableLimit), 1);

                if (ikLink.enableLimit)
                {
                    file.read(reinterpret_cast<char*>(&ikLink.limitMin), 12);
                    file.read(reinterpret_cast<char*>(&ikLink.limitMax), 12);
                }
            }
        }

        if (!file)
            return false;
    }

    return true;
}

bool PmxLoad::readMorph(PMXFileData& data, std::ifstream& file)
{
    uint32_t numOfMorph = 0;
    file.read(reinterpret_cast<char*>(&numOfMorph), sizeof(uint32_t));

    data.morphs.resize(numOfMorph);

    for (auto& morph : data.morphs)
    {
        //! name
        getPMXStringUTF16(file, morph.name);
        getPMXStringUTF8(file, morph.englishName);

        file.read(reinterpret_cast<char*>(&morph.controlPanel), 1);
        file.read(reinterpret_cast<char*>(&morph.morphType), 1);

        uint32_t dataCount = 0;
        file.read(reinterpret_cast<char*>(&dataCount), 4);

        switch (morph.morphType)
        {
        case PMXMorphType::Position:
            morph.positionMorph.resize(dataCount);
            for (auto& d : morph.positionMorph)
            {
                d.vertexIndex = readIndex(file, data.header.vertexIndexSize);
                file.read(reinterpret_cast<char*>(&d.position), 12);
            }
            break;

        case PMXMorphType::UV:
        case PMXMorphType::AddUV1:
        case PMXMorphType::AddUV2:
        case PMXMorphType::AddUV3:
        case PMXMorphType::AddUV4:
            morph.uvMorph.resize(dataCount);
            for (auto& d : morph.uvMorph)
            {
                d.vertexIndex = readIndex(file, data.header.vertexIndexSize);
                file.read(reinterpret_cast<char*>(&d.uv), 16);
            }
            break;

        case PMXMorphType::Bone:
            morph.boneMorph.resize(dataCount);
            for (auto& d : morph.boneMorph)
            {
                d.boneIndex = readIndex(file, data.header.boneIndexSize);
                file.read(reinterpret_cast<char*>(&d.position), 12);
                file.read(reinterpret_cast<char*>(&d.quaternion), 16);
            }
            break;

        case PMXMorphType::Material:
            morph.materialMorph.resize(dataCount);
            for (auto& d : morph.materialMorph)
            {
                d.materialIndex = readIndex(file, data.header.materialIndexSize);
                file.read(reinterpret_cast<char*>(&d.opType), 1);
                file.read(reinterpret_cast<char*>(&d.diffuse), 16);
                file.read(reinterpret_cast<char*>(&d.specular), 12);
                file.read(reinterpret_cast<char*>(&d.specularPower), 4);
                file.read(reinterpret_cast<char*>(&d.ambient), 12);
                file.read(reinterpret_cast<char*>(&d.edgeColor), 16);
                file.read(reinterpret_cast<char*>(&d.edgeSize), 4);
                file.read(reinterpret_cast<char*>(&d.textureFactor), 16);
                file.read(reinterpret_cast<char*>(&d.sphereTextureFactor), 16);
                file.read(reinterpret_cast<char*>(&d.toonTextureFactor), 16);
            }
            break;

        case PMXMorphType::Group:
        case PMXMorphType::Flip:
            morph.groupMorph.resize(dataCount);
            for (auto& d : morph.groupMorph)
            {
                d.morphIndex = readIndex(file, data.header.morphIndexSize);
                file.read(reinterpret_cast<char*>(&d.weight), 4);
            }
            break;

        case PMXMorphType::Impluse:
            morph.impulseMorph.resize(dataCount);
            for (auto& d : morph.impulseMorph)
            {
                d.rigidBodyIndex = readIndex(file, data.header.rigidBodyIndexSize);
                file.read(reinterpret_cast<char*>(&d.localFlag), 1);
                file.read(reinterpret_cast<char*>(&d.translateVelocity), 12);
                file.read(reinterpret_cast<char*>(&d.rotateTorque), 12);
            }
            break;

        default:
            return false;
        }

        if (!file)
            return false;
    }

    return true;
}

bool PmxLoad::readDisplayFrame(PMXFileData& data, std::ifstream& file)
{
    uint32_t numOfDisplayFrame = 0;
    file.read(reinterpret_cast<char*>(&numOfDisplayFrame), sizeof(uint32_t));

    data.displayFrames.resize(numOfDisplayFrame);

    for (auto& frame : data.displayFrames)
    {
        //! name
        getPMXStringUTF16(file, frame.name);
        getPMXStringUTF8(file, frame.englishName);

        file.read(reinterpret_cast<char*>(&frame.flag), 1);

        uint32_t targetCount = 0;
        file.read(reinterpret_cast<char*>(&targetCount), sizeof(uint32_t));

        frame.targets.resize(targetCount);
        for (auto& target : frame.targets)
        {
            file.read(reinterpret_cast<char*>(&target.type), 1);

            if (target.type == PMXDisplayFrame::TargetType::BoneIndex)
            {
                target.index = readIndex(file, data.header.boneIndexSize);
            }
            else if (target.type == PMXDisplayFrame::TargetType::MorphIndex)
            {
                target.index = readIndex(file, data.header.morphIndexSize);
            }
            else
            {
                return false;
            }
        }

        if (!file)
            return false;
    }

    return true;
}

bool PmxLoad::readRigidBody(PMXFileData& data, std::ifstream& file)
{
    uint32_t numOfRigidBody = 0;
    file.read(reinterpret_cast<char*>(&numOfRigidBody), sizeof(uint32_t));

    data.rigidBodies.resize(numOfRigidBody);

    for (auto& rb : data.rigidBodies)
    {
        //! name
        getPMXStringUTF16(file, rb.name);
        getPMXStringUTF8(file, rb.englishName);

        rb.boneIndex = readIndex(file, data.header.boneIndexSize);

        file.read(reinterpret_cast<char*>(&rb.group), 1);
        file.read(reinterpret_cast<char*>(&rb.collisionGroup), 2);
        file.read(reinterpret_cast<char*>(&rb.shape), 1);
        file.read(reinterpret_cast<char*>(&rb.shapeSize), 12);
        file.read(reinterpret_cast<char*>(&rb.translate), 12);
        file.read(reinterpret_cast<char*>(&rb.rotate), 12);
        file.read(reinterpret_cast<char*>(&rb.mass), 4);
        file.read(reinterpret_cast<char*>(&rb.translateDimmer), 4);
        file.read(reinterpret_cast<char*>(&rb.rotateDimmer), 4);
        file.read(reinterpret_cast<char*>(&rb.repulsion), 4);
        file.read(reinterpret_cast<char*>(&rb.friction), 4);
        file.read(reinterpret_cast<char*>(&rb.op), 1);

        if (!file)
            return false;
    }

    return true;
}

bool PmxLoad::readJoint(PMXFileData& data, std::ifstream& file)
{
    uint32_t numOfJoint = 0;
    file.read(reinterpret_cast<char*>(&numOfJoint), sizeof(uint32_t));

    data.joints.resize(numOfJoint);

    for (auto& joint : data.joints)
    {
        //! name
        getPMXStringUTF16(file, joint.name);
        getPMXStringUTF8(file, joint.englishName);

        file.read(reinterpret_cast<char*>(&joint.type), 1);

        joint.rigidBodyAIndex = readIndex(file, data.header.rigidBodyIndexSize);
        joint.rigidBodyBIndex = readIndex(file, data.header.rigidBodyIndexSize);

        file.read(reinterpret_cast<char*>(&joint.translate), 12);
        file.read(reinterpret_cast<char*>(&joint.rotate), 12);

        file.read(reinterpret_cast<char*>(&joint.translateLowerLimit), 12);
        file.read(reinterpret_cast<char*>(&joint.translateUpperLimit), 12);
        file.read(reinterpret_cast<char*>(&joint.rotateLowerLimit), 12);
        file.read(reinterpret_cast<char*>(&joint.rotateUpperLimit), 12);

        file.read(reinterpret_cast<char*>(&joint.springTranslateFactor), 12);
        file.read(reinterpret_cast<char*>(&joint.springRotateFactor), 12);

        if (!file)
            return false;
    }

    return true;
}

bool PmxLoad::readSoftBody(PMXFileData& data, std::ifstream& file)
{
    uint32_t numOfSoftBody = 0;
    file.read(reinterpret_cast<char*>(&numOfSoftBody), sizeof(uint32_t));

    data.softBodies.resize(numOfSoftBody);

    for (auto& sb : data.softBodies)
    {
        //! name
        getPMXStringUTF16(file, sb.name);
        getPMXStringUTF8(file, sb.englishName);

        //! base
        file.read(reinterpret_cast<char*>(&sb.type), 1);

        sb.materialIndex = readIndex(file, data.header.materialIndexSize);

        file.read(reinterpret_cast<char*>(&sb.group), 1);
        file.read(reinterpret_cast<char*>(&sb.collisionGroup), 2);
        file.read(reinterpret_cast<char*>(&sb.flag), 1);

        file.read(reinterpret_cast<char*>(&sb.bLinkLength), 4);
        file.read(reinterpret_cast<char*>(&sb.numClusters), 4);

        file.read(reinterpret_cast<char*>(&sb.totalMass), 4);
        file.read(reinterpret_cast<char*>(&sb.collisionMargin), 4);

        file.read(reinterpret_cast<char*>(&sb.areoModel), 4);

        //! physics parameters
        file.read(reinterpret_cast<char*>(&sb.vcf), 4);
        file.read(reinterpret_cast<char*>(&sb.dp), 4);
        file.read(reinterpret_cast<char*>(&sb.dg), 4);
        file.read(reinterpret_cast<char*>(&sb.lf), 4);
        file.read(reinterpret_cast<char*>(&sb.pr), 4);
        file.read(reinterpret_cast<char*>(&sb.vc), 4);
        file.read(reinterpret_cast<char*>(&sb.df), 4);
        file.read(reinterpret_cast<char*>(&sb.mt), 4);
        file.read(reinterpret_cast<char*>(&sb.chr), 4);
        file.read(reinterpret_cast<char*>(&sb.khr), 4);
        file.read(reinterpret_cast<char*>(&sb.shr), 4);
        file.read(reinterpret_cast<char*>(&sb.ahr), 4);

        //! cluster
        file.read(reinterpret_cast<char*>(&sb.srhr_cl), 4);
        file.read(reinterpret_cast<char*>(&sb.skhr_cl), 4);
        file.read(reinterpret_cast<char*>(&sb.sshr_cl), 4);
        file.read(reinterpret_cast<char*>(&sb.sr_splt_cl), 4);
        file.read(reinterpret_cast<char*>(&sb.sk_splt_cl), 4);
        file.read(reinterpret_cast<char*>(&sb.ss_splt_cl), 4);

        //! iteration
        file.read(reinterpret_cast<char*>(&sb.v_it), 4);
        file.read(reinterpret_cast<char*>(&sb.p_it), 4);
        file.read(reinterpret_cast<char*>(&sb.d_it), 4);
        file.read(reinterpret_cast<char*>(&sb.c_it), 4);

        //! stiffness
        file.read(reinterpret_cast<char*>(&sb.lst), 4);
        file.read(reinterpret_cast<char*>(&sb.ast), 4);
        file.read(reinterpret_cast<char*>(&sb.vst), 4);

        //! anchor rigid body
        uint32_t anchorCount = 0;
        file.read(reinterpret_cast<char*>(&anchorCount), sizeof(uint32_t));

        sb.anchorRigidBodies.resize(anchorCount);
        for (auto& anchor : sb.anchorRigidBodies)
        {
            anchor.rigidBodyIndex = readIndex(file, data.header.rigidBodyIndexSize);
            anchor.vertexIndex = readIndex(file, data.header.vertexIndexSize);
            file.read(reinterpret_cast<char*>(&anchor.nearMode), 1);
        }

        //! pin vertex
        uint32_t pinVertexCount = 0;
        file.read(reinterpret_cast<char*>(&pinVertexCount), sizeof(uint32_t));

        sb.pinVertexIndices.resize(pinVertexCount);
        for (auto& idx : sb.pinVertexIndices)
        {
            idx = readIndex(file, data.header.vertexIndexSize);
        }

        if (!file)
            return false;
    }

    return true;
}

void PmxLoad::readIndex(std::ifstream& file, int indexSize, int& out)
{
    switch (indexSize)
    {
    case 1:
    {
        int8_t v;
        file.read(reinterpret_cast<char*>(&v), 1);
        out = v;
        break;
    }
    case 2:
    {
        int16_t v;
        file.read(reinterpret_cast<char*>(&v), 2);
        out = v;
        break;
    }
    case 4:
    {
        int32_t v;
        file.read(reinterpret_cast<char*>(&v), 4);
        out = v;
        break;
    }
    default:
        out = -1;
        break;
    }
}

int PmxLoad::readIndex(std::ifstream& file, uint8_t indexSize)
{
    switch (indexSize)
    {
    case 1:
    {
        int8_t v;
        file.read(reinterpret_cast<char*>(&v), 1);
        return v;
    }
    case 2:
    {
        int16_t v;
        file.read(reinterpret_cast<char*>(&v), 2);
        return v;
    }
    case 4:
    {
        int32_t v;
        file.read(reinterpret_cast<char*>(&v), 4);
        return v;
    }
    default:
        return -1;
    }
}

inline bool PmxLoad::hasBoneFlag(PMXBoneFlags flags, PMXBoneFlags test)
{
    return (static_cast<uint16_t>(flags) &
        static_cast<uint16_t>(test)) != 0;
}