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

    //! 3Dサウンド再生
    SoundVoice* play3D(const std::string& name, const Vector3& worldPos, bool loop = false);

    //! マスターボリューム設定
    void setMasterVolume(float v);

    //! リバーブウェット量設定
    void setReverbWet(float wet);

    //! リスナー手動設定
    void setListenerTransform(const Vector3& position, const Vector3& forward, const Vector3& up, const Vector3& velocity);

    //! オーディオデバッグ表示
    void renderDebugContents();

private:

    struct ManagedVoice
    {
        uint64_t id = 0;
        std::string cueName;
        std::unique_ptr<SoundVoice> voice;
    };

    void updateListenerFromCamera(float deltaTime);

    std::unordered_map<std::string, std::unique_ptr<SoundBuffer>> m_buffers;
    std::vector<ManagedVoice> m_voices;
    float m_masterVolume = 1.0f;
    float m_reverbWet = 0.25f;
    bool m_manualListenerEnabled = false;
    bool m_freezeListener = false;
    Vector3 m_listenerPosition = Vector3::Zero;
    Vector3 m_listenerForward = Vector3::Forward;
    Vector3 m_listenerUp = Vector3::Up;
    Vector3 m_listenerVelocity = Vector3::Zero;
    Vector3 m_prevListenerPosition = Vector3::Zero;
    bool m_listenerHistoryValid = false;
    uint64_t m_nextVoiceId = 1;
};