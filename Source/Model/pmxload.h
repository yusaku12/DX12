#pragma once

//=====================================================
// PmxLoad クラス
//=====================================================
class PmxLoad
{
private:

    //! PMXのボーンウェイトタイプ
    enum class PMXVertexWeight : uint8_t
    {
        BDEF1,
        BDEF2,
        BDEF4,
        QDEF,
        SDEF
    };

    //! PMXファイルのマテリアル「描画モード（Draw flags）」
    enum PMXDrawModeFlags : uint8_t
    {
        PMX_DRAW_DOUBLE_SIDED = 1 << 0,
        PMX_DRAW_CAST_GROUND_SHADOW = 1 << 1,
        PMX_DRAW_CAST_SELF_SHADOW_MAP = 1 << 2,
        PMX_DRAW_RECEIVE_SELF_SHADOW = 1 << 3,
        PMX_DRAW_EDGE_ENABLED = 1 << 4
    };

    //! マテリアルのToonMode
    enum class PMXToonMode : uint8_t
    {
        Separate,
        Common
    };

    //! 「スフィアモード」は、環境マップ（sphere map）の使い方
    enum class PMXSphereMode : uint8_t
    {
        None = 0,   // 無効
        Mul = 1,    // 乗算
        Add = 2,    // 加算
        SubTex = 3  // サブテクスチャ
    };

    //! ボーンのオプション
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

    //! モーフタイプ
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

    //! PMXファイルのヘッダー情報
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

    //! PMXモデル情報構造体
    struct PMXModelInfo
    {
        std::wstring modelName;
        std::string  englishModelName;
        std::wstring comment;
        std::string  englishComment;
    };

    //! PMX頂点情報読み込み
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

    //! フェイス構造体
    struct PMXFace
    {
        int vertices[3];
    };

    //! テクスチャ構造体
    struct PMXTexture
    {
        std::wstring textureName;
    };

    //! マテリアル構造体
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

    //! IKで使用する構造体
    struct PMXIKLink
    {
        unsigned int ikBoneIndex;
        unsigned char enableLimit;

        Vector3 limitMin;
        Vector3 limitMax;
    };

    //! Bone構造体
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

    //! モーフ構造体
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

    //! 表示パネルらしい
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

    //! 剛体読み込み
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

    //! ジョイント構造体
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

    //! ソフトボディ構造体
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

    //! PMXモデルの構造体
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

    //! PMXモデル読み込み
    bool pmxLoadFile(const std::wstring& filePath, PMXFileData& fileData);

    //! PMXヘッダー情報読み込み
    bool readHeader(PMXFileData& data, std::ifstream& file);

    //! PMXモデル情報読み込み
    bool readModelInfo(PMXFileData& data, std::ifstream& file);

    //! PMXモデル頂点情報読み込み
    bool readVertex(PMXFileData& data, std::ifstream& file);

    //! PMXモデルフェイス読み込み
    bool readFace(PMXFileData& data, std::ifstream& file);

    //! テクスチャ情報読み込み
    bool readTextures(PMXFileData& data, std::ifstream& file);

    //! マテリアル情報読み込み
    bool readMaterial(PMXFileData& data, std::ifstream& file);

    //! ボーン情報読み込み
    bool readBone(PMXFileData& data, std::ifstream& file);

    //! モーフ情報読み込み
    bool readMorph(PMXFileData& data, std::ifstream& file);

    //! 表示パネル情報読み込み
    bool readDisplayFrame(PMXFileData& data, std::ifstream& file);

    //! 剛体読み込み
    bool readRigidBody(PMXFileData& data, std::ifstream& file);

    //! ジョイント読み込み
    bool readJoint(PMXFileData& data, std::ifstream& file);

    //! ソフトボディ読み込み
    bool readSoftBody(PMXFileData& data, std::ifstream& file);

    static constexpr std::array<unsigned char, 4> PMX_MAGIC_NUMBER{ 0x50, 0x4d, 0x58, 0x20 };  //!< PMXファイルのマジックナンバー
};