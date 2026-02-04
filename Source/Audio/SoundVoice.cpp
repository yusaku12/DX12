#include "pch.h"
#include "SoundVoice.h"

void SoundVoice::update(float deltaTime)
{
    if (!m_voice)
        return;

    XAUDIO2_VOICE_STATE state{};
    m_voice->GetState(&state);

    //! 再生終了検出
    if (state.BuffersQueued == 0)
    {
        stop();
        return;
    }

    //! フェード処理
    if (m_fadeDuration > 0.0f)
    {
        m_fadeTimer += deltaTime;
        float t = m_fadeTimer / m_fadeDuration;
        t = std::clamp(t, 0.0f, 1.0f);

        float vol = m_fadeIn ? t : (1.0f - t);
        m_voice->SetVolume(vol * m_volume);

        if (t >= 1.0f)
        {
            if (!m_fadeIn)
                stop(); // FadeOut後は停止

            m_fadeDuration = 0.0f;
        }
    }
}

bool SoundVoice::play(const SoundBuffer& buffer, bool loop)
{
    auto* xa = AudioEngine::Instance().getXAudio();
    auto* master = AudioEngine::Instance().getMasterVoice();

    XAUDIO2_SEND_DESCRIPTOR send{};
    send.pOutputVoice = master;
    send.Flags = 0;

    XAUDIO2_VOICE_SENDS sends{};
    sends.SendCount = 1;
    sends.pSends = &send;

    if (FAILED(xa->CreateSourceVoice(
        &m_voice,
        &buffer.getFormat(),
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        nullptr,
        &sends)))
        return false;

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = buffer.getData().data();
    buf.AudioBytes = (UINT32)buffer.getData().size();
    buf.Flags = XAUDIO2_END_OF_STREAM;
    if (loop) buf.LoopCount = XAUDIO2_LOOP_INFINITE;

    m_voice->SubmitSourceBuffer(&buf);
    m_voice->Start();

    m_playing = true;
    return true;
}

void SoundVoice::stop()
{
    if (!m_voice)
        return;

    m_voice->Stop();
    m_voice->DestroyVoice();
    m_voice = nullptr;
    m_playing = false;
}

void SoundVoice::pause()
{
    if (m_voice)
    {
        m_voice->Stop();
    }
}

void SoundVoice::resume()
{
    if (m_voice)
    {
        m_voice->Start();
    }
}

void SoundVoice::setVolume(float v)
{
    m_volume = v;
    if (m_voice)
    {
        m_voice->SetVolume(v);
    }
}

void SoundVoice::fadeIn(float time)
{
    m_fadeIn = true;
    m_fadeTimer = 0;
    m_fadeDuration = time;
}

void SoundVoice::fadeOut(float time)
{
    m_fadeIn = false;
    m_fadeTimer = 0;
    m_fadeDuration = time;
}

bool SoundVoice::isPlaying()
{
    if (!m_voice) return false;

    XAUDIO2_VOICE_STATE s{};
    m_voice->GetState(&s);
    return s.BuffersQueued > 0;
}

void SoundVoice::set3D(const Vector3& listener, const Vector3& pos)
{
    if (!m_voice)
        return;

    float dist = (listener - pos).Length();

    float volume = 1.0f / (1.0f + dist * 0.1f);
    m_voice->SetVolume(volume * m_volume);

    float pan[2] = { 0.5f,0.5f };
    float dx = pos.x - listener.x;
    pan[0] = 0.5f - dx * 0.01f;
    pan[1] = 0.5f + dx * 0.01f;

    m_voice->SetOutputMatrix(AudioEngine::Instance().getMasterVoice(), 1, 2, pan);
}