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
