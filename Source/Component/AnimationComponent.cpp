#include "pch.h"
#include "AnimationComponent.h"
#include "FbxRenderComponent.h"
#include "Model\FBXLoad.h"
#include "GameObject\GameObject.h"
#include "imgui_neo_sequencer.h"

void AnimationComponent::awake()
{
    auto* fbxRender = gameObject()->getComponent<FbxRenderComponent>();
    if (fbxRender)
    {
        m_model = fbxRender->getModel();
    }
}

void AnimationComponent::update()
{
    if (!m_model || !m_playing || m_paused) return;

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (m_animationIndex < 0 || m_animationIndex >= static_cast<int>(animations.size())) return;

    const auto& anim = animations[m_animationIndex];
    float dt = TimeManager::Instance().getDeltaTime() * m_speed;

    // 現在のアニメーションの時間を進める
    m_currentTime += dt;

    if (m_currentTime >= anim.secondsLength)
    {
        if (m_loop)
        {
            m_currentTime = std::fmod(m_currentTime, anim.secondsLength);
        }
        else
        {
            m_currentTime = anim.secondsLength;
            m_playing = false;
            m_finished = true;
        }
    }

    // ボーンにポーズを適用
    auto& bones = const_cast<std::vector<Model::Bone>&>(m_model->getBone());

    if (m_fading)
    {
        // クロスフェード中: prev と current をブレンド
        m_fadeElapsed += dt;
        float t = std::clamp(m_fadeElapsed / m_fadeDuration, 0.0f, 1.0f);

        if (m_prevAnimIndex >= 0 && m_prevAnimIndex < static_cast<int>(animations.size()))
        {
            const auto& prevAnim = animations[m_prevAnimIndex];
            m_prevTime += dt;
            // prev 側もループ or クランプ
            if (m_prevTime >= prevAnim.secondsLength)
            {
                m_prevTime = std::fmod(m_prevTime, prevAnim.secondsLength);
            }
        }

        // prev のポーズ
        std::vector<Model::Bone> prevBones = bones;
        evaluateAnimation(m_prevAnimIndex, m_prevTime, prevBones);

        // current のポーズ
        std::vector<Model::Bone> currBones = bones;
        evaluateAnimation(m_animationIndex, m_currentTime, currBones);

        // ブレンドして書き込み
        blendBones(prevBones, currBones, t, bones);

        // フェード完了判定
        if (m_fadeElapsed >= m_fadeDuration)
        {
            m_fading = false;
            m_prevAnimIndex = -1;
        }
    }
    else
    {
        evaluateAnimation(m_animationIndex, m_currentTime, bones);
    }

    // 完了コールバック
    if (m_finished && m_onFinished)
    {
        // コールバック内で play() される可能性があるため、先にコピーして呼ぶ
        auto callback = std::move(m_onFinished);
        m_onFinished = nullptr;
        callback();
    }
}

void AnimationComponent::addAnimation(const char* filename)
{
    if (!m_model) return;

    auto& resource = m_model->getResource();
    auto* fbx = dynamic_cast<FbxLoad*>(resource.get());
    if (!fbx) return;

    fbx->addAnimation(filename);
}

void AnimationComponent::play(int animationIndex, bool loop, float speed)
{
    if (!m_model) return;

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return;

    m_fading = false;
    m_prevAnimIndex = -1;
    m_animationIndex = animationIndex;
    m_currentTime = 0.0f;
    m_speed = speed;
    m_loop = loop;
    m_playing = true;
    m_paused = false;
    m_finished = false;
}

void AnimationComponent::play(const std::string& animationName, bool loop, float speed)
{
    int index = findAnimationIndex(animationName);
    if (index >= 0)
        play(index, loop, speed);
}

void AnimationComponent::crossFade(int animationIndex, float fadeDuration, bool loop, float speed)
{
    if (!m_model) return;

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return;

    // 同じアニメーションへのフェードは無視
    if (animationIndex == m_animationIndex && m_playing) return;

    // 現在再生中なら prev に退避
    if (m_playing && m_animationIndex >= 0)
    {
        if (m_fading)
        {
            // 現在のブレンド結果をボーンに確定させてから prev に引き継ぐ
            float t = std::clamp(m_fadeElapsed / m_fadeDuration, 0.0f, 1.0f);
            auto& bones = const_cast<std::vector<Model::Bone>&>(m_model->getBone());

            std::vector<Model::Bone> prevBones = bones;
            evaluateAnimation(m_prevAnimIndex, m_prevTime, prevBones);

            std::vector<Model::Bone> currBones = bones;
            evaluateAnimation(m_animationIndex, m_currentTime, currBones);

            blendBones(prevBones, currBones, t, bones);
        }

        m_prevAnimIndex = m_animationIndex;
        m_prevTime = m_currentTime;
        m_fading = true;
        m_fadeDuration = std::max(fadeDuration, 0.001f);
        m_fadeElapsed = 0.0f;
    }
    else
    {
        m_fading = false;
    }

    m_animationIndex = animationIndex;
    m_currentTime = 0.0f;
    m_speed = speed;
    m_loop = loop;
    m_playing = true;
    m_paused = false;
    m_finished = false;
}

void AnimationComponent::crossFade(const std::string& animationName, float fadeDuration, bool loop, float speed)
{
    int index = findAnimationIndex(animationName);
    if (index >= 0)
        crossFade(index, fadeDuration, loop, speed);
}

void AnimationComponent::stop()
{
    m_playing = false;
    m_paused = false;
    m_finished = false;
    m_fading = false;
}

float AnimationComponent::getNormalizedTime() const
{
    if (!m_model || m_animationIndex < 0) return 0.0f;

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (m_animationIndex >= static_cast<int>(animations.size())) return 0.0f;

    float length = animations[m_animationIndex].secondsLength;
    return (length > 0.0f) ? m_currentTime / length : 0.0f;
}

const std::string& AnimationComponent::getCurrentAnimationName() const
{
    if (!m_model || m_animationIndex < 0) return s_emptyString;

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (m_animationIndex >= static_cast<int>(animations.size())) return s_emptyString;

    return animations[m_animationIndex].name;
}

int AnimationComponent::findAnimationIndex(const std::string& name) const
{
    if (!m_model) return -1;

    const auto& animations = m_model->getResource()->getModelData().animations;
    for (int i = 0; i < static_cast<int>(animations.size()); ++i)
    {
        if (animations[i].name == name)
            return i;
    }
    return -1;
}

void AnimationComponent::evaluateAnimation(int animIndex, float time,
    std::vector<Model::Bone>& bones) const
{
    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animIndex < 0 || animIndex >= static_cast<int>(animations.size())) return;

    const auto& anim = animations[animIndex];
    const auto& keyframes = anim.keyframes;
    if (keyframes.empty()) return;

    if (keyframes.size() == 1)
    {
        const auto& keys = keyframes[0].nodeKeys;
        size_t count = std::min(bones.size(), keys.size());
        for (size_t i = 0; i < count; ++i)
        {
            bones[i].scale = keys[i].scale;
            bones[i].rotate = keys[i].rotate;
            bones[i].translate = keys[i].translate;
        }
        return;
    }

    float clampedTime = std::clamp(time, 0.0f, anim.secondsLength);

    // 現在時間が含まれる区間を探す
    size_t frame0 = keyframes.size() - 2;
    size_t frame1 = keyframes.size() - 1;
    float  t = 1.0f;

    for (size_t i = 0; i < keyframes.size() - 1; ++i)
    {
        if (clampedTime <= keyframes[i + 1].seconds)
        {
            frame0 = i;
            frame1 = i + 1;
            float span = keyframes[frame1].seconds - keyframes[frame0].seconds;
            t = (span > 0.0f) ? (clampedTime - keyframes[frame0].seconds) / span : 0.0f;
            break;
        }
    }

    const auto& keys0 = keyframes[frame0].nodeKeys;
    const auto& keys1 = keyframes[frame1].nodeKeys;
    size_t count = std::min({ bones.size(), keys0.size(), keys1.size() });

    for (size_t i = 0; i < count; ++i)
    {
        XMVECTOR s0 = XMLoadFloat3(&keys0[i].scale);
        XMVECTOR s1 = XMLoadFloat3(&keys1[i].scale);
        XMVECTOR r0 = XMLoadFloat4(&keys0[i].rotate);
        XMVECTOR r1 = XMLoadFloat4(&keys1[i].rotate);
        XMVECTOR t0 = XMLoadFloat3(&keys0[i].translate);
        XMVECTOR t1 = XMLoadFloat3(&keys1[i].translate);

        XMStoreFloat3(&bones[i].scale, XMVectorLerp(s0, s1, t));
        XMStoreFloat4(&bones[i].rotate, XMQuaternionSlerp(r0, r1, t));
        XMStoreFloat3(&bones[i].translate, XMVectorLerp(t0, t1, t));
    }
}

void AnimationComponent::blendBones(
    const std::vector<Model::Bone>& a,
    const std::vector<Model::Bone>& b,
    float t,
    std::vector<Model::Bone>& out)
{
    size_t count = std::min({ a.size(), b.size(), out.size() });
    for (size_t i = 0; i < count; ++i)
    {
        XMVECTOR sA = XMLoadFloat3(&a[i].scale);
        XMVECTOR sB = XMLoadFloat3(&b[i].scale);
        XMVECTOR rA = XMLoadFloat4(&a[i].rotate);
        XMVECTOR rB = XMLoadFloat4(&b[i].rotate);
        XMVECTOR tA = XMLoadFloat3(&a[i].translate);
        XMVECTOR tB = XMLoadFloat3(&b[i].translate);

        XMStoreFloat3(&out[i].scale, XMVectorLerp(sA, sB, t));
        XMStoreFloat4(&out[i].rotate, XMQuaternionSlerp(rA, rB, t));
        XMStoreFloat3(&out[i].translate, XMVectorLerp(tA, tB, t));
    }
}

float AnimationComponent::getSamplingTime(int animIndex) const
{
    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animIndex < 0 || animIndex >= static_cast<int>(animations.size()))
        return 1.0f / 60.0f;

    const auto& kf = animations[animIndex].keyframes;
    if (kf.size() > 1)
        return kf[1].seconds - kf[0].seconds;

    return 1.0f / 60.0f;
}

void AnimationComponent::drawSequencer()
{
    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animations.empty()) return;

    int32_t endFrame = 1;
    if (m_animationIndex >= 0 && m_animationIndex < static_cast<int>(animations.size()))
    {
        endFrame = std::max(1, static_cast<int32_t>(animations[m_animationIndex].keyframes.size()) - 1);
    }

    float samplingTime = getSamplingTime(m_animationIndex);
    m_seqCurrentFrame = static_cast<int32_t>(m_currentTime / samplingTime);

    int32_t startFrame = 0;

    if (ImGui::BeginNeoSequencer("##AnimSeq", &m_seqCurrentFrame, &startFrame, &endFrame,
        ImVec2(0, 200),
        ImGuiNeoSequencerFlags_AlwaysShowHeader))
    {
        for (int animIdx = 0; animIdx < static_cast<int>(animations.size()); ++animIdx)
        {
            const auto& anim = animations[animIdx];

            if (ImGui::BeginNeoTimelineEx(anim.name.c_str()))
            {
                int32_t firstFrame = 0;
                int32_t lastFrame = std::max(0, static_cast<int32_t>(anim.keyframes.size()) - 1);
                ImGui::NeoKeyframe(&firstFrame);
                ImGui::NeoKeyframe(&lastFrame);

                if (ImGui::IsNeoTimelineSelected(ImGuiNeoTimelineIsSelectedFlags_NewlySelected))
                {
                    crossFade(animIdx, 0.2f, m_loop, m_speed);
                }

                ImGui::EndNeoTimeLine();
            }
        }

        ImGui::EndNeoSequencer();
    }

    // ヘッドドラッグ → 再生時間に反映
    if (m_animationIndex >= 0 && m_animationIndex < static_cast<int>(animations.size()))
    {
        int32_t expectedFrame = static_cast<int32_t>(m_currentTime / samplingTime);
        if (m_seqCurrentFrame != expectedFrame)
        {
            float maxTime = animations[m_animationIndex].secondsLength;
            m_currentTime = std::clamp(m_seqCurrentFrame * samplingTime, 0.0f, maxTime);

            auto& bones = const_cast<std::vector<Model::Bone>&>(m_model->getBone());
            evaluateAnimation(m_animationIndex, m_currentTime, bones);
        }
    }
}

void AnimationComponent::drawDebugInfo()
{
    const auto& animations = m_model->getResource()->getModelData().animations;

    // 現在の再生情報
    if (m_animationIndex >= 0 && m_animationIndex < static_cast<int>(animations.size()))
    {
        const auto& anim = animations[m_animationIndex];

        ImGui::Text("Playing : %s", anim.name.c_str());
        ImGui::Text("Time    : %.2f / %.2f s", m_currentTime, anim.secondsLength);
        ImGui::Text("Frame   : %d / %d",
            m_seqCurrentFrame,
            static_cast<int>(anim.keyframes.size()) - 1);
    }
    else
    {
        ImGui::TextDisabled("No animation selected");
    }

    // クロスフェード状態
    if (m_fading && m_prevAnimIndex >= 0 && m_prevAnimIndex < static_cast<int>(animations.size()))
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "CrossFade");
        ImGui::Text("  From : %s", animations[m_prevAnimIndex].name.c_str());
        ImGui::Text("  To   : %s", animations[m_animationIndex].name.c_str());
    }
}

void AnimationComponent::inspectGUI()
{
    if (!m_model)
    {
        ImGui::TextDisabled("No model (FbxRenderComponent not found)");
        return;
    }

    const auto& animations = m_model->getResource()->getModelData().animations;
    ImGui::Text("Animations: %d", static_cast<int>(animations.size()));

    ImGui::SameLine();

    // アニメーション追加読み込み
    if (ImGui::Button("Load (.fbx)"))
    {
        std::vector<std::wstring> paths;
        if (Dialog::openFile(paths, L"Load Animation", L"", false) == DialogResult::OK && !paths.empty())
        {
            std::string path = toRelativePath(paths[0]);
            addAnimation(path.c_str());
            LOG_INFO("[AnimationComponent] Loaded animation: %s", path.c_str());
        }
    }

    // 再生速度
    ImGui::SliderFloat("Speed", &m_speed, 0.0f, 3.0f, "%.2f");

    // 再生制御ボタン
    ImGui::Checkbox("Loop", &m_loop);
    ImGui::SameLine();

    if (m_playing && !m_paused)
    {
        if (ImGui::Button("Pause"))  pause();
        ImGui::SameLine();
        if (ImGui::Button("Stop"))   stop();
    }
    else if (m_paused)
    {
        if (ImGui::Button("Resume")) resume();
        ImGui::SameLine();
        if (ImGui::Button("Stop"))   stop();
    }
    else if (m_animationIndex >= 0)
    {
        if (ImGui::Button("Play"))
            play(m_animationIndex, m_loop, m_speed);
    }

    ImGui::Separator();

    // デバッグ情報
    if (ImGui::TreeNodeEx("Debug Info", ImGuiTreeNodeFlags_DefaultOpen))
    {
        drawDebugInfo();
        ImGui::TreePop();
    }

    // シーケンサー
    if (ImGui::TreeNodeEx("Sequencer", ImGuiTreeNodeFlags_DefaultOpen))
    {
        drawSequencer();
        ImGui::TreePop();
    }
}