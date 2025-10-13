#include "pch.h"
#include "pmxload.h"

PmxLoad::PmxLoad(const std::wstring& filePath, PMXFileData& fileData)
{
    //! PMXモデル読み込み
    pmxLoadFile(filePath, fileData);
}

PmxLoad::~PmxLoad()
{
}

bool PmxLoad::pmxLoadFile(const std::wstring& filePath, PMXFileData& fileData)
{
    //! ファイルが無かったら
    if (filePath.empty() == true)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure file path");
        return false;
    }

    //! ファイル読み込み
    std::ifstream pmxFile(std::filesystem::path(filePath), std::ios::binary);
    if (!pmxFile.is_open())
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure read file path");
        return false;
    }

    //! PMXのファイルヘッダー情報読み込み
    bool result = readHeader(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readHeader");
        return false;
    }

    //! PMXのモデル情報はモデルの名前とコメントを読み込む
    result = readModelInfo(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readModelInfo");
        return false;
    }

    //! 頂点情報読み込み
    result = readVertex(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readVertex");
        return false;
    }

    //! フェイス情報読み込み
    result = readFace(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readFace");
        return false;
    }

    //! テクスチャ情報読み込み
    result = readTextures(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readTextures");
        return false;
    }

    //! マテリアル読み込み
    result = readMaterial(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readMaterial");
        return false;
    }

    //! ボーン情報読み込み
    result = readBone(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readBone");
        return false;
    }

    //! モーフ情報読み込み
    result = readMorph(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readMorph");
        return false;
    }

    //! 表示パネル読み込み
    result = readDisplayFrame(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readDisplayFrame");
        return false;
    }

    //! 剛体読み込み
    result = readRigidBody(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readRigidBody");
        return false;
    }

    //! ジョイント読み込み
    result = readJoint(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readJoint");
        return false;
    }

    //! ソフトボディ読み込み
    result = readSoftBody(fileData, pmxFile);
    if (result == false)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure readSoftBody");
        return false;
    }

    pmxFile.close();

    return true;
}

bool PmxLoad::readHeader(PMXFileData& data, std::ifstream& file)
{
    //! 一致しない場合はPMXファイル形式ではない
    file.read(reinterpret_cast<char*>(data.header.magic.data()), data.header.magic.size());
    if (data.header.magic != PMX_MAGIC_NUMBER)
    {
        Logger::getInstance().logCall(LogLevel::ERROR, "failure PMX file");
        return false;
    }

    //! 4バイトはファイルのバージョン
    //! 1バイトはデータのサイズ
    //! 1バイトはテキストエンコーディング
    file.read(reinterpret_cast<char*>(&data.header.version), sizeof(data.header.version));
    file.read(reinterpret_cast<char*>(&data.header.dataLength), sizeof(data.header.dataLength));
    file.read(reinterpret_cast<char*>(&data.header.textEncoding), sizeof(data.header.textEncoding));

    //! 追加UVの数
    file.read(reinterpret_cast<char*>(&data.header.addUVNum), sizeof(data.header.addUVNum));

    //! 1バイトずつモデル情報を読み込む
    file.read(reinterpret_cast<char*>(&data.header.vertexIndexSize), sizeof(data.header.vertexIndexSize));
    file.read(reinterpret_cast<char*>(&data.header.textureIndexSize), sizeof(data.header.textureIndexSize));
    file.read(reinterpret_cast<char*>(&data.header.materialIndexSize), sizeof(data.header.materialIndexSize));
    file.read(reinterpret_cast<char*>(&data.header.boneIndexSize), sizeof(data.header.boneIndexSize));
    file.read(reinterpret_cast<char*>(&data.header.morphIndexSize), sizeof(data.header.morphIndexSize));
    file.read(reinterpret_cast<char*>(&data.header.rigidBodyIndexSize), sizeof(data.header.rigidBodyIndexSize));

    return true;
}

bool PmxLoad::readModelInfo(PMXFileData& data, std::ifstream& file)
{
    GetPMXStringUTF16(file, data.modelInfo.modelName);
    GetPMXStringUTF8(file, data.modelInfo.englishModelName);
    GetPMXStringUTF16(file, data.modelInfo.comment);
    GetPMXStringUTF8(file, data.modelInfo.englishComment);

    return true;
}

bool PmxLoad::readVertex(PMXFileData& data, std::ifstream& file)
{
    //! 読み込んだモデルの頂点数
    unsigned int vertexCount;
    file.read(reinterpret_cast<char*>(&vertexCount), 4);
    data.vertices.resize(vertexCount);

    for (auto& vertex : data.vertices)
    {
        //! 位置、法線、UV値
        file.read(reinterpret_cast<char*>(&vertex.position), 12);
        file.read(reinterpret_cast<char*>(&vertex.normal), 12);
        file.read(reinterpret_cast<char*>(&vertex.uv), 8);

        //! 追加UV
        for (int i = 0; i < data.header.addUVNum; i++)
        {
            file.read(reinterpret_cast<char*>(&vertex.additionalUV[i]), 16);
        }

        //!  ボーンのウェイトタイプ
        file.read(reinterpret_cast<char*>(&vertex.weightType), 1);
        const unsigned char boneIndexSize = data.header.boneIndexSize;
        switch (vertex.weightType)
        {
        case PMXVertexWeight::BDEF1:
            file.read(reinterpret_cast<char*>(&vertex.boneIndices[0]), boneIndexSize);
            break;
        case PMXVertexWeight::BDEF2:
            file.read(reinterpret_cast<char*>(&vertex.boneIndices[0]), boneIndexSize);
            file.read(reinterpret_cast<char*>(&vertex.boneIndices[1]), boneIndexSize);
            file.read(reinterpret_cast<char*>(&vertex.boneWeights[0]), 4);
            break;
        case PMXVertexWeight::BDEF4:
        case PMXVertexWeight::QDEF:
            file.read(reinterpret_cast<char*>(&vertex.boneIndices[0]), boneIndexSize);
            file.read(reinterpret_cast<char*>(&vertex.boneIndices[1]), boneIndexSize);
            file.read(reinterpret_cast<char*>(&vertex.boneIndices[2]), boneIndexSize);
            file.read(reinterpret_cast<char*>(&vertex.boneIndices[3]), boneIndexSize);
            file.read(reinterpret_cast<char*>(&vertex.boneWeights[0]), 4);
            file.read(reinterpret_cast<char*>(&vertex.boneWeights[1]), 4);
            file.read(reinterpret_cast<char*>(&vertex.boneWeights[2]), 4);
            file.read(reinterpret_cast<char*>(&vertex.boneWeights[3]), 4);
            break;
        case PMXVertexWeight::SDEF:
            file.read(reinterpret_cast<char*>(&vertex.boneIndices[0]), boneIndexSize);
            file.read(reinterpret_cast<char*>(&vertex.boneIndices[1]), boneIndexSize);
            file.read(reinterpret_cast<char*>(&vertex.boneWeights[0]), 4);
            file.read(reinterpret_cast<char*>(&vertex.sdefC), 12);
            file.read(reinterpret_cast<char*>(&vertex.sdefR0), 12);
            file.read(reinterpret_cast<char*>(&vertex.sdefR1), 12);
            break;
        default:
            return false;
        }

        file.read(reinterpret_cast<char*>(&vertex.edgeMag), 4);
    }

    return true;
}

bool PmxLoad::readFace(PMXFileData& data, std::ifstream& file)
{
    int faceCount = 0;
    file.read(reinterpret_cast<char*>(&faceCount), 4);

    faceCount /= 3;
    data.faces.resize(faceCount);

    switch (data.header.vertexIndexSize)
    {
    case 1:
    {
        std::vector<uint8_t> vertices(faceCount * 3);
        file.read(reinterpret_cast<char*>(vertices.data()), sizeof(uint8_t) * vertices.size());
        for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++)
        {
            data.faces[faceIdx].vertices[0] = vertices[faceIdx * 3 + 0];
            data.faces[faceIdx].vertices[1] = vertices[faceIdx * 3 + 1];
            data.faces[faceIdx].vertices[2] = vertices[faceIdx * 3 + 2];
        }
    }
    break;
    case 2:
    {
        std::vector<uint16_t> vertices(faceCount * 3);
        file.read(reinterpret_cast<char*>(vertices.data()), sizeof(uint16_t) * vertices.size());
        for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++)
        {
            data.faces[faceIdx].vertices[0] = vertices[faceIdx * 3 + 0];
            data.faces[faceIdx].vertices[1] = vertices[faceIdx * 3 + 1];
            data.faces[faceIdx].vertices[2] = vertices[faceIdx * 3 + 2];
        }
    }
    break;
    case 4:
    {
        std::vector<uint32_t> vertices(faceCount * 3);
        file.read(reinterpret_cast<char*>(vertices.data()), sizeof(uint32_t) * vertices.size());
        for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++)
        {
            data.faces[faceIdx].vertices[0] = vertices[faceIdx * 3 + 0];
            data.faces[faceIdx].vertices[1] = vertices[faceIdx * 3 + 1];
            data.faces[faceIdx].vertices[2] = vertices[faceIdx * 3 + 2];
        }
    }
    break;
    default:
        return false;
    }

    return true;
}

bool PmxLoad::readTextures(PMXFileData& data, std::ifstream& file)
{
    unsigned int numOfTexture = 0;
    file.read(reinterpret_cast<char*>(&numOfTexture), 4);

    data.textures.resize(numOfTexture);

    for (auto& texture : data.textures)
    {
        GetPMXStringUTF16(file, texture.textureName);
    }

    return true;
}

bool PmxLoad::readMaterial(PMXFileData& data, std::ifstream& file)
{
    int numOfMaterial = 0;
    file.read(reinterpret_cast<char*>(&numOfMaterial), 4);

    data.materials.resize(numOfMaterial);

    for (auto& mat : data.materials)
    {
        GetPMXStringUTF16(file, mat.name);
        GetPMXStringUTF8(file, mat.englishName);

        file.read(reinterpret_cast<char*>(&mat.diffuse), 16);
        file.read(reinterpret_cast<char*>(&mat.specular), 12);
        file.read(reinterpret_cast<char*>(&mat.specularPower), 4);
        file.read(reinterpret_cast<char*>(&mat.ambient), 12);

        file.read(reinterpret_cast<char*>(&mat.drawMode), 1);

        file.read(reinterpret_cast<char*>(&mat.edgeColor), 16);
        file.read(reinterpret_cast<char*>(&mat.edgeSize), 4);

        file.read(reinterpret_cast<char*>(&mat.textureIndex), data.header.textureIndexSize);
        file.read(reinterpret_cast<char*>(&mat.sphereTextureIndex), data.header.textureIndexSize);
        file.read(reinterpret_cast<char*>(&mat.sphereMode), 1);

        file.read(reinterpret_cast<char*>(&mat.toonMode), 1);

        if (mat.toonMode == PMXToonMode::Separate)
        {
            file.read(reinterpret_cast<char*>(&mat.toonTextureIndex), data.header.textureIndexSize);
        }
        else if (mat.toonMode == PMXToonMode::Common)
        {
            file.read(reinterpret_cast<char*>(&mat.toonTextureIndex), 1);
        }
        else
        {
            return false;
        }

        GetPMXStringUTF16(file, mat.memo);

        file.read(reinterpret_cast<char*>(&mat.numFaceVertices), 4);
    }

    return true;
}

bool PmxLoad::readBone(PMXFileData& data, std::ifstream& file)
{
    unsigned int numOfBone;
    file.read(reinterpret_cast<char*>(&numOfBone), 4);

    data.bones.resize(numOfBone);

    for (auto& bone : data.bones)
    {
        GetPMXStringUTF16(file, bone.name);
        GetPMXStringUTF8(file, bone.englishName);

        file.read(reinterpret_cast<char*>(&bone.position), 12);
        file.read(reinterpret_cast<char*>(&bone.parentBoneIndex), data.header.boneIndexSize);
        file.read(reinterpret_cast<char*>(&bone.deformDepth), 4);

        file.read(reinterpret_cast<char*>(&bone.boneFlag), 2);

        if (((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::TargetShowMode) == 0)
        {
            file.read(reinterpret_cast<char*>(&bone.positionOffset), 12);
        }
        else
        {
            file.read(reinterpret_cast<char*>(&bone.linkBoneIndex), data.header.boneIndexSize);
        }

        if (((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::AppendRotate) ||
            ((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::AppendTranslate))
        {
            file.read(reinterpret_cast<char*>(&bone.appendBoneIndex), data.header.boneIndexSize);
            file.read(reinterpret_cast<char*>(&bone.appendWeight), 4);
        }

        if ((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::FixedAxis)
        {
            file.read(reinterpret_cast<char*>(&bone.fixedAxis), 12);
        }

        if ((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::LocalAxis)
        {
            file.read(reinterpret_cast<char*>(&bone.localXAxis), 12);
            file.read(reinterpret_cast<char*>(&bone.localZAxis), 12);
        }

        if ((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::DeformOuterParent)
        {
            file.read(reinterpret_cast<char*>(&bone.keyValue), 4);
        }

        if ((uint16_t)bone.boneFlag & (uint16_t)PMXBoneFlags::IK)
        {
            file.read(reinterpret_cast<char*>(&bone.ikTargetBoneIndex), data.header.boneIndexSize);
            file.read(reinterpret_cast<char*>(&bone.ikIterationCount), 4);
            file.read(reinterpret_cast<char*>(&bone.ikLimit), 4);

            unsigned int linkCount = 0;
            file.read(reinterpret_cast<char*>(&linkCount), 4);

            bone.ikLinks.resize(linkCount);
            for (auto& ikLink : bone.ikLinks)
            {
                file.read(reinterpret_cast<char*>(&ikLink.ikBoneIndex), data.header.boneIndexSize);
                file.read(reinterpret_cast<char*>(&ikLink.enableLimit), 1);

                if (ikLink.enableLimit != 0)
                {
                    file.read(reinterpret_cast<char*>(&ikLink.limitMin), 12);
                    file.read(reinterpret_cast<char*>(&ikLink.limitMax), 12);
                }
            }
        }
    }

    return true;
}

bool PmxLoad::readMorph(PMXFileData& data, std::ifstream& file)
{
    unsigned int numOfMorph = 0;
    file.read(reinterpret_cast<char*>(&numOfMorph), 4);

    data.morphs.resize(numOfMorph);

    for (auto& morph : data.morphs)
    {
        GetPMXStringUTF16(file, morph.name);
        GetPMXStringUTF8(file, morph.englishName);

        file.read(reinterpret_cast<char*>(&morph.controlPanel), 1);
        file.read(reinterpret_cast<char*>(&morph.morphType), 1);

        unsigned int dataCount;
        file.read(reinterpret_cast<char*>(&dataCount), 4);

        if (morph.morphType == PMXMorphType::Position)
        {
            morph.positionMorph.resize(dataCount);
            for (auto& morphData : morph.positionMorph)
            {
                file.read(reinterpret_cast<char*>(&morphData.vertexIndex), data.header.vertexIndexSize);
                file.read(reinterpret_cast<char*>(&morphData.position), 12);
            }
        }
        else if (morph.morphType == PMXMorphType::UV ||
            morph.morphType == PMXMorphType::AddUV1 ||
            morph.morphType == PMXMorphType::AddUV2 ||
            morph.morphType == PMXMorphType::AddUV3 ||
            morph.morphType == PMXMorphType::AddUV4)
        {
            morph.uvMorph.resize(dataCount);
            for (auto& morphData : morph.uvMorph)
            {
                file.read(reinterpret_cast<char*>(&morphData.vertexIndex), data.header.vertexIndexSize);
                file.read(reinterpret_cast<char*>(&morphData.uv), 16);
            }
        }
        else if (morph.morphType == PMXMorphType::Bone)
        {
            morph.boneMorph.resize(dataCount);
            for (auto& morphData : morph.boneMorph)
            {
                file.read(reinterpret_cast<char*>(&morphData.boneIndex), data.header.boneIndexSize);
                file.read(reinterpret_cast<char*>(&morphData.position), 12);
                file.read(reinterpret_cast<char*>(&morphData.quaternion), 16);
            }
        }
        else if (morph.morphType == PMXMorphType::Material)
        {
            morph.materialMorph.resize(dataCount);
            for (auto& morphData : morph.materialMorph)
            {
                file.read(reinterpret_cast<char*>(&morphData.materialIndex), data.header.materialIndexSize);
                file.read(reinterpret_cast<char*>(&morphData.opType), 1);
                file.read(reinterpret_cast<char*>(&morphData.diffuse), 16);
                file.read(reinterpret_cast<char*>(&morphData.specular), 12);
                file.read(reinterpret_cast<char*>(&morphData.specularPower), 4);
                file.read(reinterpret_cast<char*>(&morphData.ambient), 12);
                file.read(reinterpret_cast<char*>(&morphData.edgeColor), 16);
                file.read(reinterpret_cast<char*>(&morphData.edgeSize), 4);
                file.read(reinterpret_cast<char*>(&morphData.textureFactor), 16);
                file.read(reinterpret_cast<char*>(&morphData.sphereTextureFactor), 16);
                file.read(reinterpret_cast<char*>(&morphData.toonTextureFactor), 16);
            }
        }
        else if (morph.morphType == PMXMorphType::Group)
        {
            morph.groupMorph.resize(dataCount);
            for (auto& morphData : morph.groupMorph)
            {
                file.read(reinterpret_cast<char*>(&morphData.morphIndex), data.header.morphIndexSize);
                file.read(reinterpret_cast<char*>(&morphData.weight), 4);
            }
        }
        else if (morph.morphType == PMXMorphType::Flip)
        {
            morph.flipMorph.resize(dataCount);
            for (auto& morphData : morph.flipMorph)
            {
                file.read(reinterpret_cast<char*>(&morphData.morphIndex), data.header.morphIndexSize);
                file.read(reinterpret_cast<char*>(&morphData.weight), 4);
            }
        }
        else if (morph.morphType == PMXMorphType::Impluse)
        {
            morph.impulseMorph.resize(dataCount);
            for (auto& morphData : morph.impulseMorph)
            {
                file.read(reinterpret_cast<char*>(&morphData.rigidBodyIndex), data.header.rigidBodyIndexSize);
                file.read(reinterpret_cast<char*>(&morphData.localFlag), 1);
                file.read(reinterpret_cast<char*>(&morphData.translateVelocity), 12);
                file.read(reinterpret_cast<char*>(&morphData.rotateTorque), 12);
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool PmxLoad::readDisplayFrame(PMXFileData& data, std::ifstream& file)
{
    unsigned int numOfDisplayFrame = 0;
    file.read(reinterpret_cast<char*>(&numOfDisplayFrame), 4);

    data.displayFrames.resize(numOfDisplayFrame);

    for (auto& displayFrame : data.displayFrames)
    {
        GetPMXStringUTF16(file, displayFrame.name);
        GetPMXStringUTF8(file, displayFrame.englishName);

        file.read(reinterpret_cast<char*>(&displayFrame.flag), 1);

        unsigned int targetCount = 0;
        file.read(reinterpret_cast<char*>(&targetCount), 4);

        displayFrame.targets.resize(targetCount);
        for (auto& target : displayFrame.targets)
        {
            file.read(reinterpret_cast<char*>(&target.type), 1);
            if (target.type == PMXDisplayFrame::TargetType::BoneIndex)
            {
                file.read(reinterpret_cast<char*>(&target.index), data.header.boneIndexSize);
            }
            else if (target.type == PMXDisplayFrame::TargetType::MorphIndex)
            {
                file.read(reinterpret_cast<char*>(&target.index), data.header.morphIndexSize);
            }
            else
            {
                return false;
            }
        }
    }

    return true;
}

bool PmxLoad::readRigidBody(PMXFileData& data, std::ifstream& file)
{
    unsigned int numOfRigidBody = 0;
    file.read(reinterpret_cast<char*>(&numOfRigidBody), 4);

    data.rigidBodies.resize(numOfRigidBody);

    for (auto& rigidBody : data.rigidBodies)
    {
        GetPMXStringUTF16(file, rigidBody.name);
        GetPMXStringUTF8(file, rigidBody.englishName);

        file.read(reinterpret_cast<char*>(&rigidBody.boneIndex), data.header.boneIndexSize);
        file.read(reinterpret_cast<char*>(&rigidBody.group), 1);
        file.read(reinterpret_cast<char*>(&rigidBody.collisionGroup), 2);
        file.read(reinterpret_cast<char*>(&rigidBody.shape), 1);
        file.read(reinterpret_cast<char*>(&rigidBody.shapeSize), 12);
        file.read(reinterpret_cast<char*>(&rigidBody.translate), 12);
        file.read(reinterpret_cast<char*>(&rigidBody.rotate), 12);
        file.read(reinterpret_cast<char*>(&rigidBody.mass), 4);
        file.read(reinterpret_cast<char*>(&rigidBody.translateDimmer), 4);
        file.read(reinterpret_cast<char*>(&rigidBody.rotateDimmer), 4);
        file.read(reinterpret_cast<char*>(&rigidBody.repulsion), 4);
        file.read(reinterpret_cast<char*>(&rigidBody.friction), 4);
        file.read(reinterpret_cast<char*>(&rigidBody.op), 1);
    }

    return true;
}

bool PmxLoad::readJoint(PMXFileData& data, std::ifstream& file)
{
    unsigned int numOfJoint = 0;
    file.read(reinterpret_cast<char*>(&numOfJoint), 4);

    data.joints.resize(numOfJoint);

    for (auto& joint : data.joints)
    {
        GetPMXStringUTF16(file, joint.name);
        GetPMXStringUTF8(file, joint.englishName);

        file.read(reinterpret_cast<char*>(&joint.type), 1);
        file.read(reinterpret_cast<char*>(&joint.rigidBodyAIndex), data.header.rigidBodyIndexSize);
        file.read(reinterpret_cast<char*>(&joint.rigidBodyBIndex), data.header.rigidBodyIndexSize);

        file.read(reinterpret_cast<char*>(&joint.translate), 12);
        file.read(reinterpret_cast<char*>(&joint.rotate), 12);

        file.read(reinterpret_cast<char*>(&joint.translateLowerLimit), 12);
        file.read(reinterpret_cast<char*>(&joint.translateUpperLimit), 12);
        file.read(reinterpret_cast<char*>(&joint.rotateLowerLimit), 12);
        file.read(reinterpret_cast<char*>(&joint.rotateUpperLimit), 12);

        file.read(reinterpret_cast<char*>(&joint.springTranslateFactor), 12);
        file.read(reinterpret_cast<char*>(&joint.springRotateFactor), 12);
    }

    return true;
}

bool PmxLoad::readSoftBody(PMXFileData& data, std::ifstream& file)
{
    unsigned int numOfSoftBody = 0;
    file.read(reinterpret_cast<char*>(&numOfSoftBody), 4);

    data.softBodies.resize(numOfSoftBody);

    for (auto& softBody : data.softBodies)
    {
        GetPMXStringUTF16(file, softBody.name);
        GetPMXStringUTF8(file, softBody.englishName);

        file.read(reinterpret_cast<char*>(&softBody.type), 1);

        file.read(reinterpret_cast<char*>(&softBody.materialIndex), data.header.materialIndexSize);
        file.read(reinterpret_cast<char*>(&softBody.group), 1);
        file.read(reinterpret_cast<char*>(&softBody.collisionGroup), 2);

        file.read(reinterpret_cast<char*>(&softBody.flag), 1);

        file.read(reinterpret_cast<char*>(&softBody.bLinkLength), 4);
        file.read(reinterpret_cast<char*>(&softBody.numClusters), 4);

        file.read(reinterpret_cast<char*>(&softBody.totalMass), 4);
        file.read(reinterpret_cast<char*>(&softBody.collisionMargin), 4);

        file.read(reinterpret_cast<char*>(&softBody.areoModel), 4);

        file.read(reinterpret_cast<char*>(&softBody.vcf), 4);
        file.read(reinterpret_cast<char*>(&softBody.dp), 4);
        file.read(reinterpret_cast<char*>(&softBody.dg), 4);
        file.read(reinterpret_cast<char*>(&softBody.lf), 4);
        file.read(reinterpret_cast<char*>(&softBody.pr), 4);
        file.read(reinterpret_cast<char*>(&softBody.vc), 4);
        file.read(reinterpret_cast<char*>(&softBody.df), 4);
        file.read(reinterpret_cast<char*>(&softBody.mt), 4);
        file.read(reinterpret_cast<char*>(&softBody.chr), 4);
        file.read(reinterpret_cast<char*>(&softBody.khr), 4);
        file.read(reinterpret_cast<char*>(&softBody.shr), 4);
        file.read(reinterpret_cast<char*>(&softBody.ahr), 4);

        file.read(reinterpret_cast<char*>(&softBody.srhr_cl), 4);
        file.read(reinterpret_cast<char*>(&softBody.skhr_cl), 4);
        file.read(reinterpret_cast<char*>(&softBody.sshr_cl), 4);
        file.read(reinterpret_cast<char*>(&softBody.sr_splt_cl), 4);
        file.read(reinterpret_cast<char*>(&softBody.sk_splt_cl), 4);
        file.read(reinterpret_cast<char*>(&softBody.ss_splt_cl), 4);

        file.read(reinterpret_cast<char*>(&softBody.v_it), 4);
        file.read(reinterpret_cast<char*>(&softBody.p_it), 4);
        file.read(reinterpret_cast<char*>(&softBody.d_it), 4);
        file.read(reinterpret_cast<char*>(&softBody.c_it), 4);

        file.read(reinterpret_cast<char*>(&softBody.lst), 4);
        file.read(reinterpret_cast<char*>(&softBody.ast), 4);
        file.read(reinterpret_cast<char*>(&softBody.vst), 4);

        unsigned int anchorCount = 0;
        file.read(reinterpret_cast<char*>(&anchorCount), 4);

        softBody.anchorRigidBodies.resize(anchorCount);
        for (auto& anchor : softBody.anchorRigidBodies)
        {
            file.read(reinterpret_cast<char*>(&anchor.rigidBodyIndex), data.header.rigidBodyIndexSize);
            file.read(reinterpret_cast<char*>(&anchor.vertexIndex), data.header.vertexIndexSize);
            file.read(reinterpret_cast<char*>(&anchor.nearMode), 1);
        }

        unsigned int pinVertexCount = 0;
        file.read(reinterpret_cast<char*>(&pinVertexCount), 4);

        softBody.pinVertexIndices.resize(pinVertexCount);
        for (auto& pinVertex : softBody.pinVertexIndices)
        {
            file.read(reinterpret_cast<char*>(&pinVertex), data.header.vertexIndexSize);
        }
    }

    return true;
}