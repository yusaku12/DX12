#pragma once

#include <xaudio2.h>

//----------------------------------
// オーディオエンジン
//----------------------------------
class AudioEngine
{
public:

    //! シングルトン
    static AudioEngine& Instance()
    {
        static AudioEngine inst;
        return inst;
    }

    //! 初期化
    bool initialize();

    //! 終了
    void shutdown();

    //! XAudio2取得
    IXAudio2* getXAudio() const { return m_xaudio.Get(); }

    //! マスターボイス取得
    IXAudio2MasteringVoice* getMasterVoice() const { return m_masterVoice; }

private:

    AudioEngine() = default;
    ~AudioEngine() = default;

    Microsoft::WRL::ComPtr<IXAudio2> m_xaudio;
    IXAudio2MasteringVoice* m_masterVoice = nullptr;
};