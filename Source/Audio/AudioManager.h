#pragma once

#include "SoundBuffer.h"
#include "SoundVoice.h"

//-------------------------
// オーディオ管理クラス
//-------------------------
class AudioManager
{
public:

    //! シングルトンインスタンス取得
    static AudioManager& Instance()
    {
        static AudioManager inst;
        return inst;
    }

    //! 初期化
    void initialize();

    //! 終了
    void shutdown();

    //! 更新
    void update(float deltaTime);

    //! サウンド読み込み
    void load(const std::string& name, const std::string& path);

    //! サウンド再生
    SoundVoice* play(const std::string& name, bool loop = false);

    //! マスターボリューム設定
    void setMasterVolume(float v);

private:

    std::unordered_map<std::string, std::unique_ptr<SoundBuffer>> m_buffers;
    std::vector<std::unique_ptr<SoundVoice>> m_voices;
    float m_masterVolume = 1.0f;
};