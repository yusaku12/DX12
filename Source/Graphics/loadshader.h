#pragma once

//! �V�F�[�_��ǂݍ��ލۂ̔���
enum class ShaderType
{
    VS,
    PS
};

//=====================================================
// LoadShader �N���X
//=====================================================
class LoadShader
{
public:

    //! �V���O���g��
    static LoadShader& getInstance()
    {
        static LoadShader instance;
        return instance;
    }

    //! �V�F�[�_�ǂݍ���
    HRESULT loadShader(const wchar_t* shaderFile, ShaderType shaderType, Microsoft::WRL::ComPtr<ID3DBlob>& shaderBlob, Microsoft::WRL::ComPtr<ID3DBlob>& errorBlob);

private:

    LoadShader() = default;
    ~LoadShader() = default;

    // �R�s�[/���[�u�֎~
    LoadShader(const LoadShader&) = delete;
    LoadShader(LoadShader&&) = delete;
    LoadShader& operator=(const LoadShader&) = delete;
    LoadShader& operator=(LoadShader&&) = delete;

    //! �V�F�[�_�ǂݍ��ݕ⏕�֐�
    HRESULT compileShaderFromFile(const wchar_t* filePath, const char* entryName, const char* shaderVersion, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob, Microsoft::WRL::ComPtr<ID3DBlob>& errorBlob);
};