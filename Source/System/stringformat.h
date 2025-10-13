#pragma once

//! ������̃t�H�[�}�b�e�B���O
template <typename... Args>
std::string stringFormat(const std::string& format, Args&&... args)
{
    // �o�b�t�@�T�C�Y���v�Z
    int size_s = std::snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args) ...);
    if (size_s < 0) throw std::runtime_error("stringFormat error");

    size_t size = static_cast<size_t>(size_s) + 1; //!< �I�[NULL��

    std::vector<char> buf(size);
    std::snprintf(buf.data(), size, format.c_str(), std::forward<Args>(args)...);
    return std::string(buf.data(), buf.data() + size - 1); //!< NULL������
}

//! UTF16�̕������std::wstring�ɕϊ�
static bool GetPMXStringUTF16(std::ifstream& _file, std::wstring& output)
{
    std::array<wchar_t, 512> wBuffer{};
    int textSize;

    _file.read(reinterpret_cast<char*>(&textSize), 4);

    _file.read(reinterpret_cast<char*>(&wBuffer), textSize);
    output = std::wstring(&wBuffer[0], &wBuffer[0] + textSize / 2);

    return true;
}

//! UTF8�̕������std::string�ɕۑ�
static bool GetPMXStringUTF8(std::ifstream& _file, std::string& output)
{
    std::array<wchar_t, 512> wBuffer{};
    int textSize;

    _file.read(reinterpret_cast<char*>(&textSize), 4);
    _file.read(reinterpret_cast<char*>(&wBuffer), textSize);

    output = std::string(&wBuffer[0], &wBuffer[0] + textSize);

    return true;
}