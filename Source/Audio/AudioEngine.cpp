#include "pch.h"
#include "AudioEngine.h"

bool AudioEngine::initialize()
{
    if (FAILED(XAudio2Create(m_xaudio.GetAddressOf(), 0)))
        return false;

    if (FAILED(m_xaudio->CreateMasteringVoice(&m_masterVoice)))
        return false;

    return true;
}

void AudioEngine::shutdown()
{
    if (m_masterVoice)
    {
        m_masterVoice->DestroyVoice();
        m_masterVoice = nullptr;
    }
    m_xaudio.Reset();
}