#pragma once

class VmdLoad
{
public:

    explicit VmdLoad();
    ~VmdLoad() {};

private:

    struct VMDHeader
    {
        char header[30];
        char modelName[20];
    };

    struct VMDMotion
    {
        std::wstring boneName;
        unsigned int frame;
        DirectX::XMFLOAT3 translate;
        DirectX::XMFLOAT4 quaternion;
        unsigned char interpolation[64];
    };

    struct VMDMorph
    {
        std::wstring blendShapeName;
        unsigned int frame;
        float weight;
    };

    struct VMDCamera
    {
        unsigned int frame;
        float distance;
        DirectX::XMFLOAT3 interest;
        DirectX::XMFLOAT3 rotate;
        unsigned char interpolation[24];
        unsigned int fov;
        unsigned char isPerspective;
    };

    struct VMDLight
    {
        unsigned int frame;
        DirectX::XMFLOAT3 color;
        DirectX::XMFLOAT3 position;
    };

    struct VMDShadow
    {
        unsigned int frame;
        unsigned char shadowType;
        float distance;
    };

    struct VMDIKInfo
    {
        std::wstring name;
        unsigned char enable;
    };

    struct VMDIK
    {
        unsigned int frame;
        unsigned char show;
        std::vector<VMDIKInfo> ikInfos;
    };

    struct VMDFileData
    {
        VMDHeader header;
        std::vector<VMDMotion> motions;
        std::vector<VMDMorph> morphs;
        std::vector<VMDCamera> cameras;
        std::vector<VMDLight> lights;
        std::vector<VMDShadow> shadows;
        std::vector<VMDIK> iks;
    };
};