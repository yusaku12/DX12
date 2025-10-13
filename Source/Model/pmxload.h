#pragma once

//=====================================================
// PmxLoad �N���X
//=====================================================
class PmxLoad
{
private:

    //! PMX�̃{�[���E�F�C�g�^�C�v
    enum class PMXVertexWeight : uint8_t
    {
        BDEF1,
        BDEF2,
        BDEF4,
        QDEF,
        SDEF
    };

    //! PMX�t�@�C���̃}�e���A���u�`�惂�[�h�iDraw flags�j�v
    enum PMXDrawModeFlags : uint8_t
    {
        PMX_DRAW_DOUBLE_SIDED = 1 << 0,
        PMX_DRAW_CAST_GROUND_SHADOW = 1 << 1,
        PMX_DRAW_CAST_SELF_SHADOW_MAP = 1 << 2,
        PMX_DRAW_RECEIVE_SELF_SHADOW = 1 << 3,
        PMX_DRAW_EDGE_ENABLED = 1 << 4
    };

    //! �}�e���A����ToonMode
    enum class PMXToonMode : uint8_t
    {
        Separate,
        Common
    };

    //! �u�X�t�B�A���[�h�v�́A���}�b�v�isphere map�j�̎g����
    enum class PMXSphereMode : uint8_t
    {
        None = 0,   // ����
        Mul = 1,    // ��Z
        Add = 2,    // ���Z
        SubTex = 3  // �T�u�e�N�X�`��
    };

    //! �{�[���̃I�v�V����
    enum class PMXBoneFlags : uint16_t
    {
        TargetShowMode = 0x0001,
        AllowRotate = 0x0002,
        AllowTranslate = 0x0004,
        Visible = 0x0008,
        AllowControl = 0x0010,
        IK = 0x0020,
        AppendLocal = 0x0080,
        AppendRotate = 0x0100,
        AppendTranslate = 0x0200,
        FixedAxis = 0x0400,
        LocalAxis = 0x800,
        DeformAfterPhysics = 0x1000,
        DeformOuterParent = 0x2000,
    };

    //! ���[�t�^�C�v
    enum class PMXMorphType : uint8_t
    {
        Group,
        Position,
        Bone,
        UV,
        AddUV1,
        AddUV2,
        AddUV3,
        AddUV4,
        Material,
        Flip,
        Impluse,
    };

    //! PMX�t�@�C���̃w�b�_�[���
    struct PMXHeader
    {
        std::array<unsigned char, 4> magic;
        float version;
        unsigned char dataLength;
        unsigned char textEncoding;      // 0 = utf-16, 1= utf - 8
        unsigned char addUVNum;          // 0 - 4
        unsigned char vertexIndexSize;   // 1 = byte, 2 = short, 4 = int
        unsigned char textureIndexSize;  // 1 = byte, 2 = short, 4 = int
        unsigned char materialIndexSize;
        unsigned char boneIndexSize;
        unsigned char morphIndexSize;
        unsigned char rigidBodyIndexSize;
    };

    //! PMX���f�����\����
    struct PMXModelInfo
    {
        std::wstring modelName;
        std::string  englishModelName;
        std::wstring comment;
        std::string  englishComment;
    };

    //! PMX���_���ǂݍ���
    struct PMXVertex
    {
        Vector3 position;
        Vector3 normal;
        Vector2 uv;
        Vector4 additionalUV[4];

        PMXVertexWeight weightType;
        int boneIndices[4];
        float boneWeights[4];
        Vector3 sdefC;
        Vector3 sdefR0;
        Vector3 sdefR1;

        float edgeMag;
    };

    //! �t�F�C�X�\����
    struct PMXFace
    {
        int vertices[3];
    };

    //! �e�N�X�`���\����
    struct PMXTexture
    {
        std::wstring textureName;
    };

    //! �}�e���A���\����
    struct PMXMaterial
    {
        std::wstring name;
        std::string englishName;

        Vector4 diffuse;
        Vector3 specular;
        float specularPower;
        Vector3 ambient;

        PMXDrawModeFlags drawMode;

        Vector4 edgeColor;
        float edgeSize;

        unsigned int textureIndex;
        unsigned int sphereTextureIndex;
        PMXSphereMode sphereMode;

        PMXToonMode toonMode;
        unsigned int toonTextureIndex;

        std::wstring memo;

        unsigned int numFaceVertices;
    };

    //! IK�Ŏg�p����\����
    struct PMXIKLink
    {
        unsigned int ikBoneIndex;
        unsigned char enableLimit;

        Vector3 limitMin;
        Vector3 limitMax;
    };

    //! Bone�\����
    struct PMXBone
    {
        std::wstring name;
        std::string englishName;

        Vector3 position;
        unsigned int parentBoneIndex;
        unsigned int deformDepth;

        PMXBoneFlags boneFlag;

        Vector3 positionOffset;
        unsigned int linkBoneIndex;

        unsigned int appendBoneIndex;
        float appendWeight;

        Vector3 fixedAxis;
        Vector3 localXAxis;
        Vector3 localZAxis;

        unsigned int keyValue;

        unsigned int ikTargetBoneIndex;
        unsigned int ikIterationCount;
        float ikLimit;

        std::vector<PMXIKLink> ikLinks;
    };

    //! ���[�t�\����
    struct PMXMorph
    {
        std::wstring name;
        std::string englishName;

        unsigned char controlPanel;
        PMXMorphType morphType;

        struct PositionMorph
        {
            unsigned int vertexIndex;
            Vector3 position;
        };

        struct UVMorph
        {
            unsigned int vertexIndex;
            Vector4 uv;
        };

        struct BoneMorph
        {
            unsigned int boneIndex;
            Vector3 position;
            Vector4 quaternion;
        };

        struct MaterialMorph
        {
            enum class OpType : uint8_t
            {
                Mul,
                Add,
            };

            unsigned int	materialIndex;
            OpType	opType;
            Vector4 diffuse;
            Vector3 specular;
            float specularPower;
            Vector3 ambient;
            Vector4 edgeColor;
            float edgeSize;
            Vector4 textureFactor;
            Vector4 sphereTextureFactor;
            Vector4 toonTextureFactor;
        };

        struct GroupMorph
        {
            unsigned int morphIndex;
            float weight;
        };

        struct FlipMorph
        {
            unsigned int morphIndex;
            float weight;
        };

        struct ImpulseMorph
        {
            unsigned int rigidBodyIndex;
            unsigned char localFlag;	//0:OFF 1:ON
            Vector3 translateVelocity;
            Vector3 rotateTorque;
        };

        std::vector<PositionMorph> positionMorph;
        std::vector<UVMorph> uvMorph;
        std::vector<BoneMorph> boneMorph;
        std::vector<MaterialMorph> materialMorph;
        std::vector<GroupMorph> groupMorph;
        std::vector<FlipMorph> flipMorph;
        std::vector<ImpulseMorph> impulseMorph;
    };

    //! �\���p�l���炵��
    struct PMXDisplayFrame
    {
        std::wstring name;
        std::string englishName;

        enum class TargetType : uint8_t
        {
            BoneIndex,
            MorphIndex,
        };
        struct Target
        {
            TargetType type;
            unsigned int index;
        };

        enum class FrameType : uint8_t
        {
            DefaultFrame,
            SpecialFrame
        };

        FrameType flag;
        std::vector<Target> targets;
    };

    //! ���̓ǂݍ���
    struct PMXRigidBody
    {
        std::wstring name;
        std::string englishName;

        unsigned int boneIndex;
        unsigned char group;
        unsigned short collisionGroup;

        enum class Shape : uint8_t
        {
            Sphere,
            Box,
            Capsule
        };

        Shape shape;
        Vector3 shapeSize;
        Vector3 translate;
        Vector3 rotate;

        float mass;
        float translateDimmer;
        float rotateDimmer;
        float repulsion;
        float friction;

        enum class Operation : uint8_t
        {
            Static,
            Dynamic,
            DynamicAndBoneMerge,
        };
        Operation op;
    };

    //! �W���C���g�\����
    struct PMXJoint
    {
        std::wstring name;
        std::string englishName;

        enum class JointType : uint8_t
        {
            SpringDOF6,
            DOF6,
            P2P,
            ConeTwist,
            Slider,
            Hinge,
        };
        JointType type;
        unsigned int rigidBodyAIndex;
        unsigned int rigidBodyBIndex;

        Vector3 translate;
        Vector3 rotate;

        Vector3 translateLowerLimit;
        Vector3 translateUpperLimit;
        Vector3 rotateLowerLimit;
        Vector3 rotateUpperLimit;

        Vector3 springTranslateFactor;
        Vector3 springRotateFactor;
    };

    //! �\�t�g�{�f�B�\����
    struct PMXSoftBody
    {
        std::wstring name;
        std::string englishName;

        enum class SoftBodyType : uint8_t
        {
            TriMesh,
            Rope
        };
        SoftBodyType type;

        unsigned int materialIndex;
        unsigned char group;
        unsigned short collisionGroup;

        enum class SoftBodyMask : uint8_t
        {
            BLink = 0x01,
            Cluster = 0x02,
            HybridLink = 0x04,
        };
        SoftBodyMask flag;

        unsigned int bLinkLength;
        unsigned int numClusters;

        float totalMass;
        float collisionMargin;

        enum class AeroModel : int32_t
        {
            kAeroModelV_TwoSided,
            kAeroModelV_OneSided,
            kAeroModelF_TwoSided,
            kAeroModelF_OneSided,
        };
        unsigned int areoModel;

        float vcf;
        float dp;
        float dg;
        float lf;
        float pr;
        float vc;
        float df;
        float mt;
        float chr;
        float khr;
        float shr;
        float ahr;

        float srhr_cl;
        float skhr_cl;
        float sshr_cl;
        float sr_splt_cl;
        float sk_splt_cl;
        float ss_splt_cl;

        unsigned int v_it;
        unsigned int p_it;
        unsigned int d_it;
        unsigned int c_it;

        float lst;
        float ast;
        float vst;

        struct AnchorRigidBody
        {
            unsigned int rigidBodyIndex;
            unsigned int vertexIndex;
            unsigned char nearMode;
        };
        std::vector<AnchorRigidBody> anchorRigidBodies;

        std::vector<unsigned int> pinVertexIndices;
    };

    //! PMX���f���̍\����
    struct PMXFileData
    {
        PMXHeader header;
        PMXModelInfo modelInfo;

        std::vector<PMXVertex> vertices;
        std::vector<PMXFace> faces;
        std::vector<PMXTexture> textures;
        std::vector<PMXMaterial> materials;
        std::vector<PMXBone> bones;
        std::vector<PMXMorph> morphs;
        std::vector<PMXDisplayFrame> displayFrames;
        std::vector<PMXRigidBody> rigidBodies;
        std::vector<PMXJoint> joints;
        std::vector<PMXSoftBody> softBodies;
    };

public:

    PmxLoad(const std::wstring& filePath, PMXFileData& fileData);
    ~PmxLoad();

private:

    //! PMX���f���ǂݍ���
    bool pmxLoadFile(const std::wstring& filePath, PMXFileData& fileData);

    //! PMX�w�b�_�[���ǂݍ���
    bool readHeader(PMXFileData& data, std::ifstream& file);

    //! PMX���f�����ǂݍ���
    bool readModelInfo(PMXFileData& data, std::ifstream& file);

    //! PMX���f�����_���ǂݍ���
    bool readVertex(PMXFileData& data, std::ifstream& file);

    //! PMX���f���t�F�C�X�ǂݍ���
    bool readFace(PMXFileData& data, std::ifstream& file);

    //! �e�N�X�`�����ǂݍ���
    bool readTextures(PMXFileData& data, std::ifstream& file);

    //! �}�e���A�����ǂݍ���
    bool readMaterial(PMXFileData& data, std::ifstream& file);

    //! �{�[�����ǂݍ���
    bool readBone(PMXFileData& data, std::ifstream& file);

    //! ���[�t���ǂݍ���
    bool readMorph(PMXFileData& data, std::ifstream& file);

    //! �\���p�l�����ǂݍ���
    bool readDisplayFrame(PMXFileData& data, std::ifstream& file);

    //! ���̓ǂݍ���
    bool readRigidBody(PMXFileData& data, std::ifstream& file);

    //! �W���C���g�ǂݍ���
    bool readJoint(PMXFileData& data, std::ifstream& file);

    //! �\�t�g�{�f�B�ǂݍ���
    bool readSoftBody(PMXFileData& data, std::ifstream& file);

    static constexpr std::array<unsigned char, 4> PMX_MAGIC_NUMBER{ 0x50, 0x4d, 0x58, 0x20 };  //!< PMX�t�@�C���̃}�W�b�N�i���o�[
};