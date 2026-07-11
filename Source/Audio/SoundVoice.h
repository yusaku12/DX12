#pragma once

#include "SoundBuffer.h"
#include "AudioEngine.h"

//-----------------------------------------------------------------------------
// サウンドボイス
//-----------------------------------------------------------------------------
class SoundVoice
{
public:

    struct DebugSnapshot
    {
        bool isPlaying = false;
        bool isPaused = false;
        bool spatialEnabled = false;
        bool hrtfEnabled = false;
        Vector3 listenerPosition = Vector3::Zero;
        Vector3 sourcePosition = Vector3::Zero;
        Vector3 sourceVelocity = Vector3::Zero;
        float distance = 0.0f;
        float attenuation = 1.0f;
        float doppler = 1.0f;
        float directLpf = 1.0f;
        float reverbLpf = 1.0f;
        float reverbSend = 0.0f;
        float baseVolume = 1.0f;
        float appliedVolume = 1.0f;
        UINT32 sourceChannels = 0;
    };

    //! 更新
    void update(float deltaTime);

    //! 再生
    bool play(const SoundBuffer& buffer, bool loop);

    //! 停止
    void stop();

    //! 一時停止
    void pause();

    //! 再開
    void resume();

    //! 音量
    void setVolume(float v);

    //! 3D位置設定
    void set3D(const Vector3& listener, const Vector3& pos);

    //! リスナー設定
    void setListener(const Vector3& position, const Vector3& forward, const Vector3& up, const Vector3& velocity);

    //! 音源ワールド座標
    void setWorldPosition(const Vector3& position);

    //! 音源速度
    void setVelocity(const Vector3& velocity);

    //! 3D空間化の有効/無効
    void enableSpatial(bool enable);

    //! HRTFライクモード有効/無効
    void enableHRTF(bool enable);

    //! 距離減衰モデル
    void setDistanceModel(float minDistance, float maxDistance, float rolloff);

    //! リバーブ送信量 [0,1]
    void setReverbSend(float wet);

    //! デバッグ情報取得
    const DebugSnapshot& getDebugSnapshot() const { return m_debug; }

    //! フェードイン
    void fadeIn(float time);

    //! フェードアウト
    void fadeOut(float time);

    //! 再生中か
    bool isPlaying();

private:

    void applySpatializationAndVolume();
    float evaluateDistanceAttenuation(float distance) const;

    IXAudio2SourceVoice* m_voice = nullptr;
    float m_volume = 1.0f;
    float m_fadeTimer = 0.0f;
    float m_fadeDuration = 0.0f;
    float m_fadeGain = 1.0f;
    bool m_fadeIn = false;
    bool m_playing = false;
    bool m_paused = false;
    bool m_spatialEnabled = false;
    bool m_hrtfEnabled = true;

    Vector3 m_listenerPosition = Vector3::Zero;
    Vector3 m_listenerForward = Vector3::Forward;
    Vector3 m_listenerUp = Vector3::Up;
    Vector3 m_listenerVelocity = Vector3::Zero;
    Vector3 m_sourcePosition = Vector3::Zero;
    Vector3 m_sourceVelocity = Vector3::Zero;

    float m_minDistance = 1.0f;
    float m_maxDistance = 60.0f;
    float m_rolloff = 1.0f;
    float m_reverbSend = 0.35f;
    float m_distanceAttenuation = 1.0f;
    float m_dopplerFactor = 1.0f;
    float m_directLpf = 1.0f;
    float m_reverbLpf = 1.0f;

    UINT32 m_sourceChannels = 0;
    std::vector<float> m_matrixCoefficients;
    std::vector<float> m_reverbMatrix;
    X3DAUDIO_LISTENER m_listener{};
    X3DAUDIO_EMITTER m_emitter{};
    X3DAUDIO_DSP_SETTINGS m_dsp{};

    DebugSnapshot m_debug;
};