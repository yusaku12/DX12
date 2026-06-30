#pragma once

#include <xaudio2.h>
#include <x3daudio.h>

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

    //! リバーブ用サブミックス取得
    IXAudio2SubmixVoice* getReverbSubmixVoice() const { return m_reverbSubmixVoice; }

    //! X3DAudio ハンドル取得
    const X3DAUDIO_HANDLE& get3DHandle() const { return m_x3dHandle; }

    //! 3DオーディオDSP計算
    bool calculate3D(
        const X3DAUDIO_LISTENER& listener,
        const X3DAUDIO_EMITTER& emitter,
        UINT32 flags,
        X3DAUDIO_DSP_SETTINGS& dsp) const;

    //! 3Dオーディオ初期化済みか
    bool has3DAudio() const { return m_x3dReady; }

    //! リバーブ有効か
    bool hasReverbSubmix() const { return m_reverbReady && m_reverbSubmixVoice != nullptr; }

    //! マスター出力チャネル数
    UINT32 getMasterInputChannels() const { return m_masterInputChannels; }

    //! リバーブサブミックス入力チャネル数
    UINT32 getReverbInputChannels() const { return m_reverbInputChannels; }

    //! マスターチャンネルマスク
    UINT32 getMasterChannelMask() const { return m_masterChannelMask; }

    //! リバーブウェット量 [0,1]
    void setReverbWetLevel(float wet);

    //! リバーブウェット量 [0,1]
    float getReverbWetLevel() const { return m_reverbWetLevel; }

private:

    using X3DAudioInitializeFn = HRESULT(STDAPICALLTYPE*)(UINT32, FLOAT32, X3DAUDIO_HANDLE);
    using X3DAudioCalculateFn = void(STDAPICALLTYPE*)(const X3DAUDIO_HANDLE, const X3DAUDIO_LISTENER*, const X3DAUDIO_EMITTER*, UINT32, X3DAUDIO_DSP_SETTINGS*);
    using CreateFXFn = HRESULT(STDAPICALLTYPE*)(REFCLSID, IUnknown**, const void*, UINT32);

    AudioEngine() = default;
    ~AudioEngine() = default;

    Microsoft::WRL::ComPtr<IXAudio2> m_xaudio;
    IXAudio2MasteringVoice* m_masterVoice = nullptr;
    IXAudio2SubmixVoice* m_reverbSubmixVoice = nullptr;
    IUnknown* m_reverbEffect = nullptr;
    X3DAUDIO_HANDLE m_x3dHandle{};
    UINT32 m_masterChannelMask = 0;
    UINT32 m_masterInputChannels = 2;
    UINT32 m_reverbInputChannels = 2;
    float m_reverbWetLevel = 0.25f;
    bool m_x3dReady = false;
    bool m_reverbReady = false;
    HMODULE m_x3dModule = nullptr;
    X3DAudioInitializeFn m_x3dInitialize = nullptr;
    X3DAudioCalculateFn m_x3dCalculate = nullptr;
    HMODULE m_xapoFxModule = nullptr;
    CreateFXFn m_createFX = nullptr;
};