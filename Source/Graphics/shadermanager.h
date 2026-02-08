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

    //! シェーダ取得
    ID3DBlob* getShaderBlob(ShaderID id) const { return m_shaderBlobs[static_cast<int>(id)].Get(); };

private:

    ShaderManager() = default;
    ~ShaderManager() = default;
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    //! シェーダ読み込み
    void loadShader(ShaderID id);

    std::array<Microsoft::WRL::ComPtr<ID3DBlob>, static_cast<int>(ShaderID::MAX)> m_shaderBlobs;
};