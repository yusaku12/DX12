#pragma once

#include "SoundBuffer.h"
#include "AudioEngine.h"

using namespace DirectX::SimpleMath;

//-----------------------------------------------------------------------------
// サウンドボイス
//-----------------------------------------------------------------------------
class SoundVoice
{
public:

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

    //! フェードイン
    void fadeIn(float time);

    //! フェードアウト
    void fadeOut(float time);

    //! 再生中か
    bool isPlaying();

private:

    IXAudio2SourceVoice* m_voice = nullptr;
    float m_volume = 1.0f;
    float m_fadeTimer = 0.0f;
    float m_fadeDuration = 0.0f;
    bool m_fadeIn = false;
    bool m_playing = false;
};