#pragma once

#include <xaudio2.h>

//---------------------------
// wavファイル読み込み
//---------------------------
class SoundBuffer
{
public:

    //! wavファイル読み込み
    bool loadWav(const std::string& path);

    //! フォーマット取得
    const WAVEFORMATEX& getFormat() const { return m_format; }

    //! データ取得
    const std::vector<BYTE>& getData() const { return m_data; }

private:

    WAVEFORMATEX m_format{};
    std::vector<BYTE> m_data;
};