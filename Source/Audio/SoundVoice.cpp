#include "pch.h"
#include "SoundVoice.h"

void SoundVoice::update(float deltaTime)
{
    if (!m_voice)
        return;

    XAUDIO2_VOICE_STATE state{};
    m_voice->GetState(&state);

    // 再生終了検出
    if (state.BuffersQueued == 0)
    {
        stop();
        return;
    }

    // フェード処理
    if (m_fadeDuration > 0.0f)
    {
        m_fadeTimer += deltaTime;
        float t = m_fadeTimer / m_fadeDuration;
        t = std::clamp(t, 0.0f, 1.0f);

        float vol = m_fadeIn ? t : (1.0f - t);
        m_fadeGain = vol;

        if (t >= 1.0f)
        {
            if (!m_fadeIn)
                stop(); // FadeOut後は停止

            m_fadeDuration = 0.0f;
        }
    }

    applySpatializationAndVolume();
}

bool SoundVoice::play(const SoundBuffer& buffer, bool loop)
{
    auto* xa = AudioEngine::Instance().getXAudio();
    auto* master = AudioEngine::Instance().getMasterVoice();

    XAUDIO2_SEND_DESCRIPTOR sendsDesc[2]{};
    sendsDesc[0].pOutputVoice = master;
    sendsDesc[0].Flags = 0;

    UINT32 sendCount = 1;
    if (AudioEngine::Instance().hasReverbSubmix())
    {
        sendsDesc[1].pOutputVoice = AudioEngine::Instance().getReverbSubmixVoice();
        sendsDesc[1].Flags = XAUDIO2_SEND_USEFILTER;
        sendCount = 2;
    }

    XAUDIO2_VOICE_SENDS sends{};
    sends.SendCount = sendCount;
    sends.pSends = sendsDesc;

    if (FAILED(xa->CreateSourceVoice(
        &m_voice,
        &buffer.getFormat(),
        XAUDIO2_VOICE_USEFILTER,
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

    m_sourceChannels = buffer.getFormat().nChannels;
    m_paused = false;
    m_fadeGain = 1.0f;
    m_playing = true;
    m_debug.sourceChannels = m_sourceChannels;

    applySpatializationAndVolume();
    return true;
}

void SoundVoice::stop()
{
    if (!m_voice)
        return;

    m_voice->Stop();
    m_voice->DestroyVoice();
    m_voice = nullptr;
    m_paused = false;
    m_playing = false;

    m_debug.isPlaying = false;
    m_debug.isPaused = false;
}

void SoundVoice::pause()
{
    if (m_voice)
    {
        m_voice->Stop();
        m_paused = true;
        m_debug.isPaused = true;
    }
}

void SoundVoice::resume()
{
    if (m_voice)
    {
        m_voice->Start();
        m_paused = false;
        m_debug.isPaused = false;
    }
}

void SoundVoice::setVolume(float v)
{
    m_volume = v;
    applySpatializationAndVolume();
}

void SoundVoice::fadeIn(float time)
{
    m_fadeIn = true;
    m_fadeTimer = 0;
    m_fadeDuration = time;
    m_fadeGain = 0.0f;
}

void SoundVoice::fadeOut(float time)
{
    m_fadeIn = false;
    m_fadeTimer = 0;
    m_fadeDuration = time;
    m_fadeGain = 1.0f;
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
    enableSpatial(true);
    setListener(listener, Vector3::Forward, Vector3::Up, Vector3::Zero);
    setWorldPosition(pos);
    applySpatializationAndVolume();
}

void SoundVoice::setListener(const Vector3& position, const Vector3& forward, const Vector3& up, const Vector3& velocity)
{
    m_listenerPosition = position;
    m_listenerForward = forward;
    m_listenerUp = up;
    m_listenerVelocity = velocity;
}

void SoundVoice::setWorldPosition(const Vector3& position)
{
    m_sourcePosition = position;
    applySpatializationAndVolume();
}

void SoundVoice::setVelocity(const Vector3& velocity)
{
    m_sourceVelocity = velocity;
}

void SoundVoice::enableSpatial(bool enable)
{
    m_spatialEnabled = enable;
    applySpatializationAndVolume();
}

void SoundVoice::enableHRTF(bool enable)
{
    m_hrtfEnabled = enable;
    applySpatializationAndVolume();
}

void SoundVoice::setDistanceModel(float minDistance, float maxDistance, float rolloff)
{
    m_minDistance = std::max(0.01f, minDistance);
    m_maxDistance = std::max(m_minDistance + 0.01f, maxDistance);
    m_rolloff = std::clamp(rolloff, 0.01f, 8.0f);
    applySpatializationAndVolume();
}

void SoundVoice::setReverbSend(float wet)
{
    m_reverbSend = std::clamp(wet, 0.0f, 1.0f);
    applySpatializationAndVolume();
}

float SoundVoice::evaluateDistanceAttenuation(float distance) const
{
    if (distance <= m_minDistance)
    {
        return 1.0f;
    }

    if (distance >= m_maxDistance)
    {
        return 0.0f;
    }

    const float normalized = (distance - m_minDistance) / (m_maxDistance - m_minDistance);
    const float attenuation = 1.0f / (1.0f + (normalized * normalized * 8.0f * m_rolloff));
    return std::clamp(attenuation, 0.0f, 1.0f);
}

void SoundVoice::applySpatializationAndVolume()
{
    if (!m_voice)
    {
        return;
    }

    const float baseVolume = std::clamp(m_volume, 0.0f, 4.0f) * std::clamp(m_fadeGain, 0.0f, 1.0f);

    m_debug.isPlaying = m_playing;
    m_debug.isPaused = m_paused;
    m_debug.spatialEnabled = m_spatialEnabled;
    m_debug.hrtfEnabled = m_hrtfEnabled;
    m_debug.listenerPosition = m_listenerPosition;
    m_debug.sourcePosition = m_sourcePosition;
    m_debug.sourceVelocity = m_sourceVelocity;
    m_debug.baseVolume = baseVolume;
    m_debug.sourceChannels = m_sourceChannels;

    const bool canUse3D =
        m_spatialEnabled
        && AudioEngine::Instance().has3DAudio()
        && m_sourceChannels == 1
        && AudioEngine::Instance().getMasterInputChannels() > 0;

    if (!canUse3D)
    {
        m_distanceAttenuation = 1.0f;
        m_dopplerFactor = 1.0f;
        m_directLpf = 1.0f;
        m_reverbLpf = 1.0f;
        m_voice->SetFrequencyRatio(1.0f);
        m_voice->SetVolume(baseVolume);

        m_debug.distance = (m_sourcePosition - m_listenerPosition).Length();
        m_debug.attenuation = m_distanceAttenuation;
        m_debug.doppler = m_dopplerFactor;
        m_debug.directLpf = m_directLpf;
        m_debug.reverbLpf = m_reverbLpf;
        m_debug.reverbSend = m_reverbSend;
        m_debug.appliedVolume = baseVolume;
        return;
    }

    const Vector3 forward = m_listenerForward.LengthSquared() > 0.0001f ? m_listenerForward : Vector3::Forward;
    const Vector3 up = m_listenerUp.LengthSquared() > 0.0001f ? m_listenerUp : Vector3::Up;

    const Vector3 toSource = m_sourcePosition - m_listenerPosition;
    const float distance = toSource.Length();
    m_distanceAttenuation = evaluateDistanceAttenuation(distance);

    m_listener.OrientFront = X3DAUDIO_VECTOR{ forward.x, forward.y, forward.z };
    m_listener.OrientTop = X3DAUDIO_VECTOR{ up.x, up.y, up.z };
    m_listener.Position = X3DAUDIO_VECTOR{ m_listenerPosition.x, m_listenerPosition.y, m_listenerPosition.z };
    m_listener.Velocity = X3DAUDIO_VECTOR{ m_listenerVelocity.x, m_listenerVelocity.y, m_listenerVelocity.z };

    m_emitter.ChannelCount = m_sourceChannels;
    m_emitter.CurveDistanceScaler = m_minDistance;
    m_emitter.DopplerScaler = 1.0f;
    m_emitter.Position = X3DAUDIO_VECTOR{ m_sourcePosition.x, m_sourcePosition.y, m_sourcePosition.z };
    m_emitter.Velocity = X3DAUDIO_VECTOR{ m_sourceVelocity.x, m_sourceVelocity.y, m_sourceVelocity.z };
    m_emitter.OrientFront = X3DAUDIO_VECTOR{ 0.0f, 0.0f, 1.0f };
    m_emitter.OrientTop = X3DAUDIO_VECTOR{ 0.0f, 1.0f, 0.0f };

    const UINT32 dstChannels = AudioEngine::Instance().getMasterInputChannels();
    m_matrixCoefficients.assign(static_cast<size_t>(m_sourceChannels) * dstChannels, 0.0f);

    m_dsp.SrcChannelCount = m_sourceChannels;
    m_dsp.DstChannelCount = dstChannels;
    m_dsp.pMatrixCoefficients = m_matrixCoefficients.data();
    m_dsp.pDelayTimes = nullptr;

    DWORD flags = X3DAUDIO_CALCULATE_MATRIX
        | X3DAUDIO_CALCULATE_DOPPLER
        | X3DAUDIO_CALCULATE_LPF_DIRECT
        | X3DAUDIO_CALCULATE_LPF_REVERB
        | X3DAUDIO_CALCULATE_REVERB;

    if (m_hrtfEnabled)
    {
        flags |= X3DAUDIO_CALCULATE_ZEROCENTER;
    }

    if (!AudioEngine::Instance().calculate3D(m_listener, m_emitter, flags, m_dsp))
    {
        m_voice->SetFrequencyRatio(1.0f);
        m_voice->SetVolume(baseVolume);
        return;
    }

    for (float& c : m_matrixCoefficients)
    {
        c *= m_distanceAttenuation;
    }

    m_dopplerFactor = std::clamp(m_dsp.DopplerFactor, 0.5f, 2.0f);
    m_directLpf = std::clamp(m_dsp.LPFDirectCoefficient, 0.0f, 1.0f);
    m_reverbLpf = std::clamp(m_dsp.LPFReverbCoefficient, 0.0f, 1.0f);

    const float directCutoff = 2.0f * sinf(X3DAUDIO_PI / 6.0f * m_directLpf);
    XAUDIO2_FILTER_PARAMETERS directFilter{};
    directFilter.Type = LowPassFilter;
    directFilter.Frequency = std::clamp(directCutoff, 0.0f, XAUDIO2_MAX_FILTER_FREQUENCY);
    directFilter.OneOverQ = XAUDIO2_DEFAULT_FILTER_ONEOVERQ;

    m_voice->SetFilterParameters(&directFilter);
    m_voice->SetFrequencyRatio(m_dopplerFactor);
    m_voice->SetOutputMatrix(
        AudioEngine::Instance().getMasterVoice(),
        m_sourceChannels,
        dstChannels,
        m_matrixCoefficients.data());

    if (AudioEngine::Instance().hasReverbSubmix())
    {
        const UINT32 reverbDstChannels = AudioEngine::Instance().getReverbInputChannels();
        m_reverbMatrix.assign(static_cast<size_t>(m_sourceChannels) * reverbDstChannels, 0.0f);

        float wet = m_dsp.ReverbLevel;
        wet = std::clamp(wet * m_reverbSend * m_distanceAttenuation, 0.0f, 1.0f);

        const float sendCoeff = wet / static_cast<float>(std::max<UINT32>(1, reverbDstChannels));
        std::fill(m_reverbMatrix.begin(), m_reverbMatrix.end(), sendCoeff);

        m_voice->SetOutputMatrix(
            AudioEngine::Instance().getReverbSubmixVoice(),
            m_sourceChannels,
            reverbDstChannels,
            m_reverbMatrix.data());

        const float reverbCutoff = 2.0f * sinf(X3DAUDIO_PI / 6.0f * m_reverbLpf);
        XAUDIO2_FILTER_PARAMETERS reverbFilter{};
        reverbFilter.Type = LowPassFilter;
        reverbFilter.Frequency = std::clamp(reverbCutoff, 0.0f, XAUDIO2_MAX_FILTER_FREQUENCY);
        reverbFilter.OneOverQ = XAUDIO2_DEFAULT_FILTER_ONEOVERQ;
        m_voice->SetOutputFilterParameters(AudioEngine::Instance().getReverbSubmixVoice(), &reverbFilter);
    }

    const float appliedVolume = baseVolume;
    m_voice->SetVolume(appliedVolume);

    m_debug.distance = distance;
    m_debug.attenuation = m_distanceAttenuation;
    m_debug.doppler = m_dopplerFactor;
    m_debug.directLpf = m_directLpf;
    m_debug.reverbLpf = m_reverbLpf;
    m_debug.reverbSend = m_reverbSend;
    m_debug.appliedVolume = appliedVolume;
}