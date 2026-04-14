#include "pch.h"
#include "AnimationStateMachine.h"

void AnimationStateMachine::initialize(Model* model)
{
    m_model = model;

    // デフォルトレイヤーが無ければ追加
    if (m_layers.empty())
    {
        AnimationLayer base;
        base.name = "Base Layer";
        base.weight = 1.0f;
        base.blendMode = LayerBlendMode::Override;
        m_layers.push_back(base);
    }

    // デフォルトステートがあれば遷移
    if (m_defaultState && !m_currentState)
    {
        m_currentState = m_defaultState;
        m_currentTime = 0.0f;
        m_normalizedTime = 0.0f;
    }
}

void AnimationStateMachine::update(float deltaTime)
{
    if (!m_model || !m_currentState) return;

    const auto& animations = m_model->getResource()->getModelData().animations;
    int animIdx = m_currentState->getAnimationIndex();
    if (animIdx < 0 || animIdx >= static_cast<int>(animations.size())) return;

    float speed = m_currentState->getSpeed();
    float length = getAnimationLength(animIdx);
    if (length <= 0.0f) return;

    float prevNorm = m_normalizedTime;

    // 再生時間を進める
    float dt = deltaTime * speed;

    switch (m_currentState->getLoopMode())
    {
    case LoopMode::Loop:
        m_currentTime += dt;
        if (m_currentTime >= length)
        {
            m_currentTime = std::fmod(m_currentTime, length);
            m_currentState->resetEvents();
        }
        break;

    case LoopMode::Once:
        m_currentTime += dt;
        if (m_currentTime >= length)
        {
            m_currentTime = length;
        }
        break;

    case LoopMode::PingPong:
        if (!m_pingPongReverse)
        {
            m_currentTime += dt;
            if (m_currentTime >= length)
            {
                m_currentTime = length;
                m_pingPongReverse = true;
            }
        }
        else
        {
            m_currentTime -= dt;
            if (m_currentTime <= 0.0f)
            {
                m_currentTime = 0.0f;
                m_pingPongReverse = false;
                m_currentState->resetEvents();
            }
        }
        break;
    }

    m_normalizedTime = (length > 0.0f) ? m_currentTime / length : 0.0f;

    // イベント発火
    fireEvents(m_currentState, prevNorm, m_normalizedTime);

    // ボーンにポーズを適用
    auto& bones = const_cast<std::vector<Model::Bone>&>(m_model->getBone());

    if (m_fading && m_prevState)
    {
        // クロスフェード中
        m_fadeElapsed += deltaTime;
        float t = std::clamp(m_fadeElapsed / m_fadeDuration, 0.0f, 1.0f);

        int prevAnimIdx = m_prevState->getAnimationIndex();
        if (prevAnimIdx >= 0 && prevAnimIdx < static_cast<int>(animations.size()))
        {
            // prev 側の時間も進める
            m_prevTime += deltaTime * m_prevState->getSpeed();
            float prevLen = getAnimationLength(prevAnimIdx);
            if (m_prevTime >= prevLen)
            {
                m_prevTime = std::fmod(m_prevTime, std::max(prevLen, 0.001f));
            }
        }

        std::vector<Model::Bone> prevBones = bones;
        evaluateAnimation(prevAnimIdx, m_prevTime, prevBones);

        std::vector<Model::Bone> currBones = bones;
        evaluateAnimation(animIdx, m_currentTime, currBones);

        blendBones(prevBones, currBones, t, bones);

        // フェード完了
        if (m_fadeElapsed >= m_fadeDuration)
        {
            m_fading = false;
            m_prevState = nullptr;
        }
    }
    else
    {
        evaluateAnimation(animIdx, m_currentTime, bones);
    }

    // レイヤーブレンド（ベースレイヤー以外）
    // TODO: 複数ステートマシンをレイヤーごとに持つ本格実装
    // 現時点ではレイヤー 0 = ベースとして、追加レイヤーのマスクブレンドを適用
    for (size_t i = 1; i < m_layers.size(); ++i)
    {
        const auto& layer = m_layers[i];
        if (layer.weight <= 0.0f || layer.boneMask.empty()) continue;

        // レイヤー用のポーズ評価（現ステートのアニメーションを使う簡易版）
        std::vector<Model::Bone> layerBones = bones;
        evaluateAnimation(animIdx, m_currentTime, layerBones);

        blendBonesWithMask(bones, layerBones, layer.weight,
            layer.boneMask, layer.blendMode, bones);
    }

    // 遷移チェック
    // Any State 遷移（最優先）
    for (auto& trans : m_anyStateTransitions)
    {
        // 自分自身への遷移は抑制
        if (m_currentState && m_currentState->getName() == trans.destStateName) continue;

        if (evaluateConditions(trans.conditions))
        {
            AnimationState* dest = findState(trans.destStateName);
            if (dest)
            {
                consumeTriggers(trans.conditions);
                executeTransition(dest, trans.fadeDuration);
                return;
            }
        }
    }

    // 現在ステートの遷移
    if (m_currentState && (!m_fading || m_currentState->getTransitions().empty() == false))
    {
        for (auto& trans : m_currentState->getTransitions())
        {
            // exitTime チェック
            if (trans.hasExitTime && m_normalizedTime < trans.exitTime) continue;

            // 遷移中に中断不可の場合はスキップ
            if (m_fading && !trans.interruptible) continue;

            if (evaluateConditions(trans.conditions))
            {
                AnimationState* dest = findState(trans.destStateName);
                if (dest)
                {
                    consumeTriggers(trans.conditions);
                    executeTransition(dest, trans.fadeDuration);
                    return;
                }
            }
        }
    }
}

AnimationState* AnimationStateMachine::addState(const std::string& name, int animIndex)
{
    // 重複チェック
    if (findState(name))
    {
        LOG_WARN("[AnimStateMachine] State '%s' already exists.", name.c_str());
        return findState(name);
    }

    auto state = std::make_unique<AnimationState>(name, animIndex);
    AnimationState* ptr = state.get();
    m_states.push_back(std::move(state));
    return ptr;
}

AnimationState* AnimationStateMachine::findState(const std::string& name)
{
    for (auto& s : m_states)
    {
        if (s->getName() == name) return s.get();
    }
    return nullptr;
}

const AnimationState* AnimationStateMachine::findState(const std::string& name) const
{
    for (auto& s : m_states)
    {
        if (s->getName() == name) return s.get();
    }
    return nullptr;
}

void AnimationStateMachine::setDefaultState(const std::string& name)
{
    m_defaultState = findState(name);
}

const std::string& AnimationStateMachine::getCurrentStateName() const
{
    return m_currentState ? m_currentState->getName() : s_emptyString;
}

const AnimationState* AnimationStateMachine::getCurrentState() const
{
    return m_currentState;
}

void AnimationStateMachine::addAnyStateTransition(const AnimationTransition& transition)
{
    m_anyStateTransitions.push_back(transition);
}

void AnimationStateMachine::addParameter(const std::string& name, AnimParamType type)
{
    AnimationParameter param;
    param.name = name;
    param.type = type;
    m_parameters[name] = param;
}

void AnimationStateMachine::setFloat(const std::string& name, float value)
{
    auto* p = findParam(name);
    if (p && p->type == AnimParamType::Float) p->floatValue = value;
}

void AnimationStateMachine::setInt(const std::string& name, int value)
{
    auto* p = findParam(name);
    if (p && p->type == AnimParamType::Int) p->intValue = value;
}

void AnimationStateMachine::setBool(const std::string& name, bool value)
{
    auto* p = findParam(name);
    if (p && p->type == AnimParamType::Bool) p->boolValue = value;
}

void AnimationStateMachine::setTrigger(const std::string& name)
{
    auto* p = findParam(name);
    if (p && p->type == AnimParamType::Trigger)
    {
        p->boolValue = true;
        p->triggerFired = false;
    }
}

float AnimationStateMachine::getFloat(const std::string& name) const
{
    const auto* p = findParam(name);
    return (p && p->type == AnimParamType::Float) ? p->floatValue : 0.0f;
}

int AnimationStateMachine::getInt(const std::string& name) const
{
    const auto* p = findParam(name);
    return (p && p->type == AnimParamType::Int) ? p->intValue : 0;
}

bool AnimationStateMachine::getBool(const std::string& name) const
{
    const auto* p = findParam(name);
    return (p && p->type == AnimParamType::Bool) ? p->boolValue : false;
}

int AnimationStateMachine::addLayer(const std::string& name, float weight, LayerBlendMode mode)
{
    AnimationLayer layer;
    layer.name = name;
    layer.weight = weight;
    layer.blendMode = mode;
    m_layers.push_back(layer);
    return static_cast<int>(m_layers.size()) - 1;
}

void AnimationStateMachine::setLayerWeight(int layerIndex, float weight)
{
    if (layerIndex >= 0 && layerIndex < static_cast<int>(m_layers.size()))
    {
        m_layers[layerIndex].weight = std::clamp(weight, 0.0f, 1.0f);
    }
}

void AnimationStateMachine::setLayerBoneMask(int layerIndex, const std::vector<int>& boneIndices)
{
    if (layerIndex >= 0 && layerIndex < static_cast<int>(m_layers.size()))
    {
        m_layers[layerIndex].boneMask = boneIndices;
    }
}

void AnimationStateMachine::forceTransition(const std::string& stateName, float fadeDuration)
{
    AnimationState* dest = findState(stateName);
    if (dest)
    {
        executeTransition(dest, fadeDuration);
    }
    else
    {
        LOG_WARN("[AnimStateMachine] State '%s' not found for forceTransition.", stateName.c_str());
    }
}

bool AnimationStateMachine::evaluateConditions(const std::vector<TransitionCondition>& conditions) const
{
    // 条件がなければ即遷移（exitTime 付きの場合はそちらでゲートされる）
    if (conditions.empty()) return true;

    for (const auto& cond : conditions)
    {
        const auto* param = findParam(cond.paramName);
        if (!param) return false;

        float value = 0.0f;
        switch (param->type)
        {
        case AnimParamType::Float:   value = param->floatValue;                break;
        case AnimParamType::Int:     value = static_cast<float>(param->intValue); break;
        case AnimParamType::Bool:    value = param->boolValue ? 1.0f : 0.0f;  break;
        case AnimParamType::Trigger: value = param->boolValue ? 1.0f : 0.0f;  break;
        }

        bool result = false;
        switch (cond.op)
        {
        case CompareOp::Greater:  result = (value > cond.threshold);          break;
        case CompareOp::Less:     result = (value < cond.threshold);          break;
        case CompareOp::Equal:    result = (std::abs(value - cond.threshold) < 0.001f); break;
        case CompareOp::NotEqual: result = (std::abs(value - cond.threshold) >= 0.001f); break;
        }

        if (!result) return false;
    }
    return true;
}

void AnimationStateMachine::executeTransition(AnimationState* destState, float fadeDuration)
{
    if (!destState) return;

    // 現在のステートを prev に退避
    if (m_currentState)
    {
        m_prevState = m_currentState;
        m_prevTime = m_currentTime;
        m_fading = true;
        m_fadeDuration = std::max(fadeDuration, 0.001f);
        m_fadeElapsed = 0.0f;
    }

    m_currentState = destState;
    m_currentTime = 0.0f;
    m_normalizedTime = 0.0f;
    m_pingPongReverse = false;
    m_currentState->resetEvents();
}

void AnimationStateMachine::evaluateAnimation(int animIndex, float time, std::vector<Model::Bone>& bones) const
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

    //! 現在時間が含まれる区間を探す
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
        XMVECTOR p0 = XMLoadFloat3(&keys0[i].translate);
        XMVECTOR p1 = XMLoadFloat3(&keys1[i].translate);

        XMStoreFloat3(&bones[i].scale, XMVectorLerp(s0, s1, t));
        XMStoreFloat4(&bones[i].rotate, XMQuaternionSlerp(r0, r1, t));
        XMStoreFloat3(&bones[i].translate, XMVectorLerp(p0, p1, t));
    }
}

void AnimationStateMachine::blendBones(const std::vector<Model::Bone>& a,
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

void AnimationStateMachine::blendBonesWithMask(const std::vector<Model::Bone>& src,
    const std::vector<Model::Bone>& layer,
    float weight,
    const std::vector<int>& mask,
    LayerBlendMode mode,
    std::vector<Model::Bone>& out)
{
    for (int idx : mask)
    {
        if (idx < 0 || idx >= static_cast<int>(out.size())) continue;
        if (idx >= static_cast<int>(layer.size())) continue;

        XMVECTOR sS = XMLoadFloat3(&src[idx].scale);
        XMVECTOR sL = XMLoadFloat3(&layer[idx].scale);
        XMVECTOR rS = XMLoadFloat4(&src[idx].rotate);
        XMVECTOR rL = XMLoadFloat4(&layer[idx].rotate);
        XMVECTOR tS = XMLoadFloat3(&src[idx].translate);
        XMVECTOR tL = XMLoadFloat3(&layer[idx].translate);

        if (mode == LayerBlendMode::Override)
        {
            XMStoreFloat3(&out[idx].scale, XMVectorLerp(sS, sL, weight));
            XMStoreFloat4(&out[idx].rotate, XMQuaternionSlerp(rS, rL, weight));
            XMStoreFloat3(&out[idx].translate, XMVectorLerp(tS, tL, weight));
        }
        else // Additive
        {
            // Additive: base + (layer - identity) * weight
            XMVECTOR identityR = XMQuaternionIdentity();
            XMVECTOR identityT = XMVectorZero();
            XMVECTOR identityS = XMVectorSet(1, 1, 1, 0);

            XMVECTOR addS = XMVectorLerp(identityS, sL, weight);
            XMVECTOR addR = XMQuaternionSlerp(identityR, rL, weight);
            XMVECTOR addT = XMVectorLerp(identityT, tL, weight);

            XMStoreFloat3(&out[idx].scale, XMVectorMultiply(sS, addS));
            XMStoreFloat4(&out[idx].rotate, XMQuaternionMultiply(rS, addR));
            XMStoreFloat3(&out[idx].translate, XMVectorAdd(tS, addT));
        }
    }
}

float AnimationStateMachine::getAnimationLength(int animIndex) const
{
    const auto& animations = m_model->getResource()->getModelData().animations;
    if (animIndex < 0 || animIndex >= static_cast<int>(animations.size())) return 0.0f;
    return animations[animIndex].secondsLength;
}

void AnimationStateMachine::fireEvents(AnimationState* state, float prevNorm, float currNorm)
{
    if (!state) return;

    for (auto& evt : state->getEvents())
    {
        if (evt.fired) continue;

        // ループ折り返しも考慮
        bool shouldFire = false;
        if (currNorm >= prevNorm)
        {
            // 通常進行
            shouldFire = (prevNorm < evt.normalizedTime && currNorm >= evt.normalizedTime);
        }
        else
        {
            // ループ折り返し（prevNorm > currNorm の場合）
            shouldFire = (prevNorm < evt.normalizedTime) || (currNorm >= evt.normalizedTime);
        }

        if (shouldFire)
        {
            evt.fired = true;
            if (evt.callback)
            {
                evt.callback();
            }
        }
    }
}

void AnimationStateMachine::consumeTriggers(const std::vector<TransitionCondition>& conditions)
{
    for (const auto& cond : conditions)
    {
        auto* param = findParam(cond.paramName);
        if (param && param->type == AnimParamType::Trigger)
        {
            param->boolValue = false;
            param->triggerFired = true;
        }
    }
}

AnimationParameter* AnimationStateMachine::findParam(const std::string& name)
{
    auto it = m_parameters.find(name);
    return (it != m_parameters.end()) ? &it->second : nullptr;
}

const AnimationParameter* AnimationStateMachine::findParam(const std::string& name) const
{
    auto it = m_parameters.find(name);
    return (it != m_parameters.end()) ? &it->second : nullptr;
}

void AnimationStateMachine::drawDebugGUI()
{
    if (!m_model) return;

    // 現在のステート情報
    if (m_currentState)
    {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "State: %s",
            m_currentState->getName().c_str());

        float length = getAnimationLength(m_currentState->getAnimationIndex());
        ImGui::Text("Time: %.2f / %.2f s", m_currentTime, length);

        // 再生プログレスバー
        ImGui::ProgressBar(m_normalizedTime, ImVec2(-1, 0),
            std::format("{:.1f}%%", m_normalizedTime * 100.0f).c_str());

        // ループモード表示
        const char* loopStr = "Unknown";
        switch (m_currentState->getLoopMode())
        {
        case LoopMode::Loop:     loopStr = "Loop";     break;
        case LoopMode::Once:     loopStr = "Once";     break;
        case LoopMode::PingPong: loopStr = "PingPong"; break;
        }
        ImGui::Text("Loop: %s  Speed: %.2f", loopStr, m_currentState->getSpeed());
    }
    else
    {
        ImGui::TextDisabled("No active state");
    }

    // クロスフェード状態
    if (m_fading && m_prevState)
    {
        ImGui::Separator();
        float fadeProgress = std::clamp(m_fadeElapsed / m_fadeDuration, 0.0f, 1.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "CrossFade");
        ImGui::Text("  %s -> %s", m_prevState->getName().c_str(),
            m_currentState ? m_currentState->getName().c_str() : "???");
        ImGui::ProgressBar(fadeProgress, ImVec2(-1, 0),
            std::format("Fade: {:.0f}%%", fadeProgress * 100.0f).c_str());
    }

    // パラメータ一覧
    if (!m_parameters.empty() && ImGui::TreeNode("Parameters"))
    {
        for (auto& [name, param] : m_parameters)
        {
            switch (param.type)
            {
            case AnimParamType::Float:
                ImGui::DragFloat(name.c_str(), &param.floatValue, 0.01f);
                break;
            case AnimParamType::Int:
                ImGui::DragInt(name.c_str(), &param.intValue);
                break;
            case AnimParamType::Bool:
                ImGui::Checkbox(name.c_str(), &param.boolValue);
                break;
            case AnimParamType::Trigger:
                if (ImGui::Button(name.c_str()))
                {
                    param.boolValue = true;
                    param.triggerFired = false;
                }
                ImGui::SameLine();
                ImGui::TextDisabled(param.boolValue ? "[Active]" : "[Idle]");
                break;
            }
        }
        ImGui::TreePop();
    }

    // ステート一覧
    if (ImGui::TreeNode("States"))
    {
        for (auto& state : m_states)
        {
            bool isCurrent = (state.get() == m_currentState);
            if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));

            if (ImGui::Selectable(state->getName().c_str(), isCurrent))
            {
                forceTransition(state->getName(), 0.2f);
            }

            if (isCurrent) ImGui::PopStyleColor();

            // 遷移先のツールチップ
            if (ImGui::IsItemHovered() && !state->getTransitions().empty())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Transitions:");
                for (const auto& t : state->getTransitions())
                {
                    ImGui::BulletText("-> %s (fade: %.2fs)", t.destStateName.c_str(), t.fadeDuration);
                }
                ImGui::EndTooltip();
            }
        }
        ImGui::TreePop();
    }

    // レイヤー一覧
    if (m_layers.size() > 1 && ImGui::TreeNode("Layers"))
    {
        for (size_t i = 0; i < m_layers.size(); ++i)
        {
            auto& layer = m_layers[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::SliderFloat(layer.name.c_str(), &layer.weight, 0.0f, 1.0f);
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}