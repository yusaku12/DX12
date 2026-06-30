#include "pch.h"

#include "Camera/CameraComponent.h"

void AudioManager::initialize()
{
    if (!AudioEngine::Instance().initialize())
    {
        LOG_ERROR("AudioManager: initialize failed.");
        return;
    }

    setMasterVolume(m_masterVolume);
    setReverbWet(m_reverbWet);
}

void AudioManager::shutdown()
{
    m_voices.clear();
    m_buffers.clear();
    m_listenerHistoryValid = false;
    AudioEngine::Instance().shutdown();
}

void AudioManager::update(float deltaTime)
{
    if (!m_manualListenerEnabled && !m_freezeListener)
    {
        updateListenerFromCamera(deltaTime);
    }

    for (auto& managed : m_voices)
    {
        if (!managed.voice)
        {
            continue;
        }

        managed.voice->setListener(m_listenerPosition, m_listenerForward, m_listenerUp, m_listenerVelocity);
        managed.voice->update(deltaTime);
    }

    // 再生終了ボイスの回収
    m_voices.erase(
        std::remove_if(m_voices.begin(), m_voices.end(),
            [](const ManagedVoice& managed)
            {
                return !managed.voice || !managed.voice->isPlaying();
            }),
        m_voices.end());
}

void AudioManager::load(const std::string& name, const std::string& path)
{
    auto buf = DXMem::makeUnique<SoundBuffer>();
    if (!buf->loadWav(path))
    {
        LOG_ERROR(std::format("AudioManager: failed to load wav '{}'.", path));
        return;
    }

    m_buffers[name] = std::move(buf);
}

SoundVoice* AudioManager::play(const std::string& name, bool loop)
{
    auto it = m_buffers.find(name);
    if (it == m_buffers.end() || !it->second)
    {
        LOG_WARN(std::format("AudioManager: cue '{}' is not loaded.", name));
        return nullptr;
    }

    auto voice = DXMem::makeUnique<SoundVoice>();
    if (!voice->play(*it->second, loop))
    {
        LOG_ERROR(std::format("AudioManager: play failed for cue '{}'.", name));
        return nullptr;
    }

    voice->setListener(m_listenerPosition, m_listenerForward, m_listenerUp, m_listenerVelocity);

    SoundVoice* ptr = voice.get();
    ManagedVoice managed{};
    managed.id = m_nextVoiceId++;
    managed.cueName = name;
    managed.voice = std::move(voice);
    m_voices.push_back(std::move(managed));
    return ptr;
}

SoundVoice* AudioManager::play3D(const std::string& name, const Vector3& worldPos, bool loop)
{
    SoundVoice* voice = play(name, loop);
    if (!voice)
    {
        return nullptr;
    }

    voice->enableSpatial(true);
    voice->enableHRTF(true);
    voice->setDistanceModel(1.0f, 80.0f, 1.0f);
    voice->setReverbSend(0.35f);
    voice->setWorldPosition(worldPos);
    voice->setListener(m_listenerPosition, m_listenerForward, m_listenerUp, m_listenerVelocity);
    return voice;
}

void AudioManager::setMasterVolume(float v)
{
    m_masterVolume = std::clamp(v, 0.0f, 4.0f);
    if (auto* master = AudioEngine::Instance().getMasterVoice())
    {
        master->SetVolume(m_masterVolume);
    }
}

void AudioManager::setReverbWet(float wet)
{
    m_reverbWet = std::clamp(wet, 0.0f, 1.0f);
    AudioEngine::Instance().setReverbWetLevel(m_reverbWet);
}

void AudioManager::setListenerTransform(const Vector3& position, const Vector3& forward, const Vector3& up, const Vector3& velocity)
{
    m_manualListenerEnabled = true;
    m_listenerPosition = position;
    m_listenerForward = forward.LengthSquared() > 0.0001f ? forward : Vector3::Forward;
    m_listenerUp = up.LengthSquared() > 0.0001f ? up : Vector3::Up;
    m_listenerVelocity = velocity;
    m_prevListenerPosition = m_listenerPosition;
    m_listenerHistoryValid = true;
}

void AudioManager::updateListenerFromCamera(float deltaTime)
{
    auto* camera = CameraManager::Instance().getMainCamera();
    if (!camera)
    {
        return;
    }

    m_listenerPosition = camera->getPosition();
    m_listenerForward = camera->getForward();
    m_listenerUp = camera->getUp();

    if (m_listenerHistoryValid && deltaTime > 0.0001f)
    {
        m_listenerVelocity = (m_listenerPosition - m_prevListenerPosition) / deltaTime;
    }
    else
    {
        m_listenerVelocity = Vector3::Zero;
        m_listenerHistoryValid = true;
    }

    m_prevListenerPosition = m_listenerPosition;
}

void AudioManager::renderDebugContents()
{
    ImGui::Text("Voices: %d", static_cast<int>(m_voices.size()));
    ImGui::Text("3D: %s", AudioEngine::Instance().has3DAudio() ? "Available" : "Unavailable");
    ImGui::Text("Reverb: %s", AudioEngine::Instance().hasReverbSubmix() ? "Enabled" : "Disabled");

    float master = m_masterVolume;
    if (ImGui::SliderFloat("Master Volume", &master, 0.0f, 2.0f, "%.2f"))
    {
        setMasterVolume(master);
    }

    float reverbWet = m_reverbWet;
    if (ImGui::SliderFloat("Global Reverb Wet", &reverbWet, 0.0f, 1.0f, "%.2f"))
    {
        setReverbWet(reverbWet);
    }

    ImGui::Checkbox("Freeze Listener", &m_freezeListener);
    ImGui::Checkbox("Manual Listener", &m_manualListenerEnabled);

    ImGui::Text("Listener Pos: %.2f %.2f %.2f", m_listenerPosition.x, m_listenerPosition.y, m_listenerPosition.z);
    ImGui::Text("Listener Fwd: %.2f %.2f %.2f", m_listenerForward.x, m_listenerForward.y, m_listenerForward.z);
    ImGui::Text("Listener Vel: %.2f %.2f %.2f", m_listenerVelocity.x, m_listenerVelocity.y, m_listenerVelocity.z);

    ImGui::Separator();

    if (ImGui::BeginTable("AudioVoicesTable", 11, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Cue");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("3D");
        ImGui::TableSetupColumn("HRTF");
        ImGui::TableSetupColumn("Dist");
        ImGui::TableSetupColumn("Attn");
        ImGui::TableSetupColumn("Doppler");
        ImGui::TableSetupColumn("LPF D");
        ImGui::TableSetupColumn("Reverb");
        ImGui::TableSetupColumn("Pos");
        ImGui::TableHeadersRow();

        for (auto& managed : m_voices)
        {
            if (!managed.voice)
            {
                continue;
            }

            const auto& dbg = managed.voice->getDebugSnapshot();

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%llu", static_cast<unsigned long long>(managed.id));

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(managed.cueName.c_str());

            ImGui::TableSetColumnIndex(2);
            if (!dbg.isPlaying)
            {
                ImGui::TextUnformatted("Stopped");
            }
            else if (dbg.isPaused)
            {
                ImGui::TextUnformatted("Paused");
            }
            else
            {
                ImGui::TextUnformatted("Playing");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(dbg.spatialEnabled ? "On" : "Off");

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(dbg.hrtfEnabled ? "On" : "Off");

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.2f", dbg.distance);

            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%.2f", dbg.attenuation);

            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%.2f", dbg.doppler);

            ImGui::TableSetColumnIndex(8);
            ImGui::Text("%.2f", dbg.directLpf);

            ImGui::TableSetColumnIndex(9);
            ImGui::Text("%.2f", dbg.reverbSend);

            ImGui::TableSetColumnIndex(10);
            ImGui::Text("%.1f %.1f %.1f", dbg.sourcePosition.x, dbg.sourcePosition.y, dbg.sourcePosition.z);
        }

        ImGui::EndTable();
    }
}