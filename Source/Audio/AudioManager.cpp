#include "pch.h"

void AudioManager::initialize()
{
    AudioEngine::Instance().initialize();
}

void AudioManager::shutdown()
{
    m_voices.clear();
    AudioEngine::Instance().shutdown();
}

void AudioManager::update(float deltaTime)
{
    for (auto& v : m_voices)
    {
        if (v)
            v->update(deltaTime);
    }

    // 再生終了ボイスの回収
    m_voices.erase(
        std::remove_if(m_voices.begin(), m_voices.end(),
            [](const std::unique_ptr<SoundVoice>& v)
            {
                return !v || !v->isPlaying();
            }),
        m_voices.end());
}

void AudioManager::load(const std::string& name, const std::string& path)
{
    auto buf = std::make_unique<SoundBuffer>();
    buf->loadWav(path);
    m_buffers[name] = std::move(buf);
}

SoundVoice* AudioManager::play(const std::string& name, bool loop)
{
    auto voice = std::make_unique<SoundVoice>();
    voice->play(*m_buffers[name], loop);

    SoundVoice* ptr = voice.get();
    m_voices.push_back(std::move(voice));
    return ptr;
}

void AudioManager::setMasterVolume(float v)
{
    m_masterVolume = v;
    AudioEngine::Instance().getMasterVoice()->SetVolume(v);
}