#include "pch.h"
#include "AudioEngine.h"
#include <xapofx.h>

bool AudioEngine::initialize()
{
    if (m_xaudio)
    {
        return true;
    }

    if (FAILED(XAudio2Create(m_xaudio.GetAddressOf(), 0)))
    {
        LOG_ERROR("AudioEngine: XAudio2Create failed.");
        return false;
    }

    if (FAILED(m_xaudio->CreateMasteringVoice(&m_masterVoice)))
    {
        LOG_ERROR("AudioEngine: CreateMasteringVoice failed.");
        shutdown();
        return false;
    }

    XAUDIO2_VOICE_DETAILS masterDetails{};
    m_masterVoice->GetVoiceDetails(&masterDetails);
    m_masterInputChannels = masterDetails.InputChannels;

    DWORD channelMask = 0;
    if (FAILED(m_masterVoice->GetChannelMask(&channelMask)))
    {
        channelMask = SPEAKER_STEREO;
        LOG_WARN("AudioEngine: Master channel mask unavailable. Fallback to stereo.");
    }
    m_masterChannelMask = static_cast<UINT32>(channelMask);

    const wchar_t* x3dCandidates[] =
    {
        L"x3daudio1_9.dll",
        L"x3daudio1_8.dll",
        L"x3daudio1_7.dll"
    };

    for (const wchar_t* dllName : x3dCandidates)
    {
        m_x3dModule = ::LoadLibraryW(dllName);
        if (m_x3dModule)
        {
            break;
        }
    }

    if (m_x3dModule)
    {
        m_x3dInitialize = reinterpret_cast<X3DAudioInitializeFn>(::GetProcAddress(m_x3dModule, "X3DAudioInitialize"));
        m_x3dCalculate = reinterpret_cast<X3DAudioCalculateFn>(::GetProcAddress(m_x3dModule, "X3DAudioCalculate"));
    }

    m_x3dReady = m_x3dInitialize != nullptr && m_x3dCalculate != nullptr
        && SUCCEEDED(m_x3dInitialize(m_masterChannelMask, X3DAUDIO_SPEED_OF_SOUND, m_x3dHandle));

    if (!m_x3dReady)
    {
        LOG_WARN("AudioEngine: X3DAudioInitialize failed. Spatialization disabled.");
    }

    const wchar_t* xapoCandidates[] =
    {
        L"XAPOFX1_5.dll",
        L"XAPOFX1_4.dll",
        L"XAPOFX1_3.dll",
        L"XAPOFX1_2.dll",
        L"XAPOFX1_1.dll",
        L"XAPOFX1_0.dll"
    };

    for (const wchar_t* dllName : xapoCandidates)
    {
        m_xapoFxModule = ::LoadLibraryW(dllName);
        if (m_xapoFxModule)
        {
            break;
        }
    }

    if (m_xapoFxModule)
    {
        m_createFX = reinterpret_cast<CreateFXFn>(::GetProcAddress(m_xapoFxModule, "CreateFX"));
    }

    IUnknown* reverbEffect = nullptr;
    HRESULT hr = E_FAIL;
    if (m_createFX)
    {
        hr = m_createFX(CLSID_FXReverb, &reverbEffect, nullptr, 0);
    }

    if (SUCCEEDED(hr) && reverbEffect)
    {
        XAUDIO2_EFFECT_DESCRIPTOR effectDesc{};
        effectDesc.InitialState = TRUE;
        effectDesc.OutputChannels = masterDetails.InputChannels;
        effectDesc.pEffect = reverbEffect;

        XAUDIO2_EFFECT_CHAIN effectChain{};
        effectChain.EffectCount = 1;
        effectChain.pEffectDescriptors = &effectDesc;

        hr = m_xaudio->CreateSubmixVoice(
            &m_reverbSubmixVoice,
            masterDetails.InputChannels,
            masterDetails.InputSampleRate,
            0,
            0,
            nullptr,
            &effectChain);

        if (SUCCEEDED(hr) && m_reverbSubmixVoice)
        {
            FXREVERB_PARAMETERS params{};
            params.Diffusion = FXREVERB_DEFAULT_DIFFUSION;
            params.RoomSize = FXREVERB_DEFAULT_ROOMSIZE;
            m_reverbSubmixVoice->SetEffectParameters(0, &params, sizeof(params));

            XAUDIO2_VOICE_DETAILS reverbDetails{};
            m_reverbSubmixVoice->GetVoiceDetails(&reverbDetails);
            m_reverbInputChannels = reverbDetails.InputChannels;

            m_reverbEffect = reverbEffect;
            m_reverbReady = true;
            setReverbWetLevel(m_reverbWetLevel);
        }
        else
        {
            reverbEffect->Release();
            LOG_WARN("AudioEngine: CreateSubmixVoice for reverb failed. Reverb disabled.");
        }
    }
    else
    {
        LOG_WARN("AudioEngine: XAudio2CreateReverb failed. Reverb disabled.");
    }

    return true;
}

void AudioEngine::shutdown()
{
    m_x3dReady = false;
    m_reverbReady = false;

    if (m_reverbSubmixVoice)
    {
        m_reverbSubmixVoice->DestroyVoice();
        m_reverbSubmixVoice = nullptr;
    }

    if (m_reverbEffect)
    {
        m_reverbEffect->Release();
        m_reverbEffect = nullptr;
    }

    m_createFX = nullptr;
    if (m_xapoFxModule)
    {
        ::FreeLibrary(m_xapoFxModule);
        m_xapoFxModule = nullptr;
    }

    m_x3dInitialize = nullptr;
    m_x3dCalculate = nullptr;
    if (m_x3dModule)
    {
        ::FreeLibrary(m_x3dModule);
        m_x3dModule = nullptr;
    }

    if (m_masterVoice)
    {
        m_masterVoice->DestroyVoice();
        m_masterVoice = nullptr;
    }
    m_xaudio.Reset();
}

void AudioEngine::setReverbWetLevel(float wet)
{
    m_reverbWetLevel = std::clamp(wet, 0.0f, 1.0f);
    if (m_reverbSubmixVoice)
    {
        m_reverbSubmixVoice->SetVolume(m_reverbWetLevel);
    }
}

bool AudioEngine::calculate3D(
    const X3DAUDIO_LISTENER& listener,
    const X3DAUDIO_EMITTER& emitter,
    UINT32 flags,
    X3DAUDIO_DSP_SETTINGS& dsp) const
{
    if (!m_x3dReady || !m_x3dCalculate)
    {
        return false;
    }

    m_x3dCalculate(m_x3dHandle, &listener, &emitter, flags, &dsp);
    return true;
}