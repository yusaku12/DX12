#include "pch.h"
#include "AnimationComponent.h"
#include "FbxRenderComponent.h"
#include "Model\FBXLoad.h"
#include "GameObject\GameObject.h"

AnimationComponent::~AnimationComponent()
{
    if (!m_model)
    {
        m_model = nullptr;
        delete m_model;
    }
}

void AnimationComponent::awake()
{
    // 同じ GameObject の FbxRenderComponent からモデルを取得
    auto* fbxRender = gameObject()->getComponent<FbxRenderComponent>();
    if (fbxRender)
    {
        m_model = fbxRender->getModel();
    }
}

void AnimationComponent::addAnimation(const char* filename)
{
    if (!m_model) return;

    // ModelResource の実体は FbxLoad なので dynamic_cast で取得
    auto& resource = m_model->getResource();
    auto* fbx = dynamic_cast<FbxLoad*>(resource.get());
    if (!fbx) return;

    fbx->addAnimation(filename);
}

void AnimationComponent::update()
{
    if (!m_model || !m_playing) return;

    if (!m_model) return;

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (m_animationIndex < 0 || m_animationIndex >= static_cast<int>(animations.size())) return;

    const auto& anim = animations[m_animationIndex];

    // 時間を進める
    m_currentTime += TimeManager::Instance().getDeltaTime();

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

    // ボーンに適用
    evaluate();
}

void AnimationComponent::play(int animationIndex, bool loop)
{
    if (!m_model) return;

    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return;

    m_animationIndex = animationIndex;
    m_currentTime = 0.0f;
    m_loop = loop;
    m_playing = true;
    m_finished = false;
}

void AnimationComponent::stop()
{
    m_playing = false;
    m_finished = false;
}

void AnimationComponent::evaluate()
{
    const auto& anim = m_model->getResource()->getModelData().animations[m_animationIndex];
    const auto& keyframes = anim.keyframes;
    if (keyframes.empty()) return;

    // 現在時間に対応する2フレームを探す
    size_t frame0 = 0;
    size_t frame1 = 0;
    float  t = 0.0f;

    for (size_t i = 0; i < keyframes.size() - 1; ++i)
    {
        if (m_currentTime >= keyframes[i].seconds && m_currentTime < keyframes[i + 1].seconds)
        {
            frame0 = i;
            frame1 = i + 1;
            float span = keyframes[frame1].seconds - keyframes[frame0].seconds;
            t = (span > 0.0f) ? (m_currentTime - keyframes[frame0].seconds) / span : 0.0f;
            break;
        }
    }

    // 最終フレーム到達時
    if (frame0 == 0 && frame1 == 0 && keyframes.size() > 1)
    {
        frame0 = keyframes.size() - 1;
        frame1 = frame0;
    }

    // ボーンに補間結果を書き込む
    const auto& keys0 = keyframes[frame0].nodeKeys;
    const auto& keys1 = keyframes[frame1].nodeKeys;
    auto& bones = const_cast<std::vector<Model::Bone>&>(m_model->getBone());
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

void AnimationComponent::inspectGUI()
{
    if (!m_model)
    {
        ImGui::TextDisabled("No model (IRenderComponent not found)");
        return;
    }

    const auto& animations = m_model->getResource()->getModelData().animations;

    ImGui::Text("Animations: %d", static_cast<int>(animations.size()));

    if (ImGui::Button("Load Animation (.fbx)"))
    {
        std::vector<std::wstring> paths;
        if (Dialog::openFile(paths, L"Load Animation", L"", false) == DialogResult::OK && !paths.empty())
        {
            std::string path = wstringToString(paths[0]);
            addAnimation(path.c_str());
            LOG_INFO("[AnimationComponent] Loaded animation: %s", path.c_str());
        }
    }

    ImGui::Separator();

    // アニメーション選択
    const char* preview = (m_animationIndex >= 0 && m_animationIndex < static_cast<int>(animations.size()))
        ? animations[m_animationIndex].name.c_str()
        : "None";

    if (ImGui::BeginCombo("Animation", preview))
    {
        for (int i = 0; i < static_cast<int>(animations.size()); ++i)
        {
            bool selected = (i == m_animationIndex);
            if (ImGui::Selectable(animations[i].name.c_str(), selected))
            {
                play(i, m_loop);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Checkbox("Loop", &m_loop);

    // 再生状態
    if (m_playing && m_animationIndex >= 0)
    {
        float length = animations[m_animationIndex].secondsLength;
        float norm = (length > 0.0f) ? m_currentTime / length : 0.0f;
        ImGui::ProgressBar(norm);
    }

    if (m_playing)
    {
        if (ImGui::Button("Stop")) stop();
    }
    else
    {
        if (m_animationIndex >= 0 && ImGui::Button("Play"))
            play(m_animationIndex, m_loop);
    }
}