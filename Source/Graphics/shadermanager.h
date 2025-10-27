#pragma once

#include "loadshader.h"

//=====================================================
// ShaderManager �N���X
//=====================================================
class ShaderManager
{
public:

    static ShaderManager& Instance();

    //! �V�F�[�_�ǂݍ���
    LoadShader* load(const std::wstring& filePath, ShaderType shaderType, D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpipeline);

    //! �ʃA�����[�h
    void unload(const std::wstring& filePath, ShaderType shaderType);

    //! �S�V�F�[�_�N���A
    void clear();

private:

    ShaderManager() = default;
    ~ShaderManager() = default;
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    //! �V�F�[�_�L���b�V��
    std::unordered_map<ShaderKey, std::unique_ptr<LoadShader>, ShaderKeyHash> m_shaderCache;
};