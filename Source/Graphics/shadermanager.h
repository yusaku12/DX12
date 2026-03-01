#pragma once

#include "ShaderData.h"

//=====================================================
// 読み込んだシェーダをキャッシュするシングルトン
//=====================================================
class ShaderManager
{
public:

    //! シングルトンインスタンス取得
    static ShaderManager& Instance()
    {
        static ShaderManager instance;
        return instance;
    }

    //! 初期化
    void initialize();

    //! 更新
    void update();

    //! シェーダ取得
    ID3DBlob* getShaderBlob(ShaderID id) const { return m_shaders[static_cast<size_t>(id)].blob.Get(); };

    //! ホットリロード感知フラグ,PSOCreatorで使用
    bool isDirty(ShaderID id) { return m_shaders[(size_t)id].dirty; }
    void clearDirty(ShaderID id) { m_shaders[(size_t)id].dirty = false; }

private:

    ShaderManager() = default;
    ~ShaderManager() = default;
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    //! シェーダ読み込み
    void loadShader(ShaderID id);

    //! シェーダホットリロード
    bool reloadShader(ShaderID id);

    //! シェーダーファイルのパス取得
    std::wstring getHlslPath(const ShaderDesc& desc) const;

    //! コンパイル済みシェーダーファイルのパス取得
    std::wstring getCsoPath(const ShaderDesc& desc) const;

    //! シェーダーのランタイムデータ
    struct ShaderRuntimeData
    {
        ShaderDesc desc;
        Microsoft::WRL::ComPtr<ID3DBlob> blob;
        bool dirty = false;
        std::filesystem::file_time_type lastWriteTime;
    };

    std::array<ShaderRuntimeData, static_cast<size_t>(ShaderID::MAX)> m_shaders;
};