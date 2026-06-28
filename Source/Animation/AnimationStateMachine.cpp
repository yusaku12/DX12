#include "pch.h"
#include "AnimationStateMachine.h"

namespace
{
    constexpr float kBlendEpsilon = 0.0001f;
}

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

    float speed = m_currentState->getSpeed();
    float length = getStateLength(*m_currentState);
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

        // prev 側の時間も進める
        m_prevTime += deltaTime * m_prevState->getSpeed();
        float prevLen = getStateLength(*m_prevState);
        if (prevLen > 0.0f && m_prevTime >= prevLen)
        {
            m_prevTime = std::fmod(m_prevTime, std::max(prevLen, 0.001f));
        }

        std::vector<Model::Bone> prevBones = bones;
        evaluateStatePose(*m_prevState, m_prevTime, prevBones);

        std::vector<Model::Bone> currBones = bones;
        evaluateStatePose(*m_currentState, m_currentTime, currBones);

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
        evaluateStatePose(*m_currentState, m_currentTime, bones);
    }

    // レイヤーブレンド（ベースレイヤー以外）
    for (size_t i = 1; i < m_layers.size(); ++i)
    {
        auto& layer = m_layers[i];
        if (!layer.enabled) continue;
        if (layer.weight <= 0.0f || layer.boneMask.empty()) continue;

        std::vector<Model::Bone> layerBones = bones;

        if (layer.useCurrentStatePose || layer.layerAnimationIndex < 0)
        {
            evaluateStatePose(*m_currentState, m_currentTime, layerBones);
        }
        else
        {
            float layerLength = getAnimationLength(layer.layerAnimationIndex);
            if (layerLength > 0.0f)
            {
                layer.layerTime += deltaTime * layer.layerSpeed;
                if (layer.layerLoop)
                {
                    layer.layerTime = std::fmod(layer.layerTime, std::max(layerLength, kBlendEpsilon));
                }
                else
                {
                    layer.layerTime = std::clamp(layer.layerTime, 0.0f, layerLength);
                }
            }

            evaluateAnimation(layer.layerAnimationIndex, layer.layerTime, layerBones);
        }

        blendBonesWithMask(bones, layerBones, layer.weight,
            layer.boneMask, layer.blendMode,
            layer.additiveAffectScale,
            layer.additiveAffectTranslation,
            bones);
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

size_t AnimationStateMachine::addLayer(const AnimationLayer& layer)
{
    AnimationLayer entry = layer;
    if (entry.name.empty())
    {
        entry.name = std::format("Layer{}", m_layers.size());
    }
    m_layers.push_back(std::move(entry));
    return m_layers.size() - 1;
}

bool AnimationStateMachine::removeLayer(size_t index)
{
    if (index == 0 || index >= m_layers.size())
    {
        return false;
    }

    m_layers.erase(m_layers.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

AnimationLayer* AnimationStateMachine::getLayer(size_t index)
{
    if (index >= m_layers.size()) return nullptr;
    return &m_layers[index];
}

const AnimationLayer* AnimationStateMachine::getLayer(size_t index) const
{
    if (index >= m_layers.size()) return nullptr;
    return &m_layers[index];
}

bool AnimationStateMachine::setLayerAnimation(size_t index, int animationIndex, float speed, bool loop)
{
    AnimationLayer* layer = getLayer(index);
    if (!layer || index == 0)
    {
        return false;
    }

    layer->useCurrentStatePose = false;
    layer->layerAnimationIndex = animationIndex;
    layer->layerSpeed = speed;
    layer->layerLoop = loop;
    layer->layerTime = 0.0f;
    return true;
}

bool AnimationStateMachine::setLayerUseCurrentStatePose(size_t index, bool useCurrentStatePose)
{
    AnimationLayer* layer = getLayer(index);
    if (!layer || index == 0)
    {
        return false;
    }

    layer->useCurrentStatePose = useCurrentStatePose;
    if (useCurrentStatePose)
    {
        layer->layerAnimationIndex = -1;
        layer->layerTime = 0.0f;
    }
    return true;
}

bool AnimationStateMachine::removeState(const std::string& name)
{
    auto it = std::find_if(m_states.begin(), m_states.end(), [&](const std::unique_ptr<AnimationState>& s) {
        return s && s->getName() == name;
    });
    if (it == m_states.end())
    {
        return false;
    }

    AnimationState* victim = it->get();

    // 各ステートの遷移から victim 行きを除去
    for (auto& s : m_states)
    {
        if (!s) continue;
        auto& transitions = s->getTransitions();
        transitions.erase(
            std::remove_if(transitions.begin(), transitions.end(),
                [&](const AnimationTransition& t) { return t.destStateName == name; }),
            transitions.end());
    }

    if (m_currentState == victim)
    {
        m_currentState = nullptr;
        m_currentTime = 0.0f;
        m_normalizedTime = 0.0f;
        m_pingPongReverse = false;
    }
    if (m_prevState == victim)
    {
        m_prevState = nullptr;
        m_fading = false;
        m_fadeElapsed = 0.0f;
    }
    if (m_defaultState == victim)
    {
        m_defaultState = nullptr;
    }

    m_states.erase(it);

    if (!m_defaultState && !m_states.empty())
    {
        m_defaultState = m_states.front().get();
    }
    if (!m_currentState && m_defaultState)
    {
        m_currentState = m_defaultState;
        m_currentTime = 0.0f;
        m_normalizedTime = 0.0f;
    }

    return true;
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

void AnimationStateMachine::clearController()
{
    m_states.clear();
    m_parameters.clear();

    m_currentState = nullptr;
    m_defaultState = nullptr;
    m_prevState = nullptr;

    m_currentTime = 0.0f;
    m_normalizedTime = 0.0f;
    m_prevTime = 0.0f;
    m_fadeDuration = 0.0f;
    m_fadeElapsed = 0.0f;
    m_fading = false;
    m_pingPongReverse = false;

    m_layers.clear();
    AnimationLayer base;
    base.name = "Base Layer";
    base.weight = 1.0f;
    base.blendMode = LayerBlendMode::Override;
    m_layers.push_back(base);
}

const std::string& AnimationStateMachine::getDefaultStateName() const
{
    return m_defaultState ? m_defaultState->getName() : s_emptyString;
}

const std::string& AnimationStateMachine::getCurrentStateName() const
{
    return m_currentState ? m_currentState->getName() : s_emptyString;
}

const AnimationState* AnimationStateMachine::getCurrentState() const
{
    return m_currentState;
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

bool AnimationStateMachine::addTransitionUnique(const std::string& fromStateName,
    const std::string& toStateName,
    float fadeDuration)
{
    AnimationState* from = findState(fromStateName);
    AnimationState* to = findState(toStateName);
    if (!from || !to) return false;

    for (const auto& t : from->getTransitions())
    {
        if (t.destStateName == toStateName)
        {
            return false;
        }
    }

    AnimationTransition t;
    t.destStateName = toStateName;
    t.fadeDuration = fadeDuration;
    from->addTransition(t);
    return true;
}

float AnimationStateMachine::getCurrentStateLength() const
{
    if (!m_currentState) return 0.0f;
    return getStateLength(*m_currentState);
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

void AnimationStateMachine::evaluateStatePose(const AnimationState& state,
    float time,
    std::vector<Model::Bone>& bones) const
{
    const BlendTreeData* blendTree = state.getBlendTree();
    if (!blendTree || blendTree->children.empty())
    {
        evaluateAnimation(state.getAnimationIndex(), time, bones);
        return;
    }

    struct BlendEntry
    {
        int animIndex = -1;
        float weight = 0.0f;
        float timeScale = 1.0f;
    };

    std::vector<BlendEntry> entries;
    entries.reserve(blendTree->children.size());

    if (blendTree->type == BlendTreeType::Blend1D)
    {
        std::vector<std::pair<float, size_t>> sorted;
        sorted.reserve(blendTree->children.size());
        for (size_t i = 0; i < blendTree->children.size(); ++i)
        {
            sorted.emplace_back(blendTree->children[i].threshold, i);
        }
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
            });

        float x = getFloat(blendTree->parameterX);
        if (sorted.size() == 1)
        {
            const auto& child = blendTree->children[sorted[0].second];
            entries.push_back({ child.animationIndex, 1.0f, child.timeScale });
        }
        else
        {
            for (size_t i = 0; i + 1 < sorted.size(); ++i)
            {
                const auto& a = blendTree->children[sorted[i].second];
                const auto& b = blendTree->children[sorted[i + 1].second];

                if (x <= a.threshold)
                {
                    entries.push_back({ a.animationIndex, 1.0f, a.timeScale });
                    break;
                }
                if (x >= b.threshold && i + 2 < sorted.size())
                {
                    continue;
                }

                float span = std::max(0.001f, b.threshold - a.threshold);
                float t = std::clamp((x - a.threshold) / span, 0.0f, 1.0f);
                entries.push_back({ a.animationIndex, 1.0f - t, a.timeScale });
                entries.push_back({ b.animationIndex, t, b.timeScale });
                break;
            }

            if (entries.empty())
            {
                const auto& child = blendTree->children[sorted.back().second];
                entries.push_back({ child.animationIndex, 1.0f, child.timeScale });
            }
        }
    }
    else
    {
        Vector2 p = { getFloat(blendTree->parameterX), getFloat(blendTree->parameterY) };

        if (blendTree->type == BlendTreeType::FreeformDirectional2D)
        {
            struct DirSample
            {
                const BlendTreeChild* child = nullptr;
                Vector2 dir = Vector2::Zero;
                float radius = 0.0f;
                float angle = 0.0f;
            };

            std::vector<const BlendTreeChild*> centerSamples;
            std::vector<DirSample> directionalSamples;
            centerSamples.reserve(blendTree->children.size());
            directionalSamples.reserve(blendTree->children.size());

            for (const auto& child : blendTree->children)
            {
                const float radius = child.position.Length();
                if (radius <= kBlendEpsilon)
                {
                    centerSamples.push_back(&child);
                    continue;
                }

                DirSample s;
                s.child = &child;
                s.radius = radius;
                s.dir = child.position / radius;
                s.angle = std::atan2(s.dir.y, s.dir.x);
                directionalSamples.push_back(s);
            }

            const float paramMag = p.Length();
            Vector2 paramDir = Vector2(1.0f, 0.0f);
            if (paramMag > kBlendEpsilon)
            {
                paramDir = p / paramMag;
            }
            const float paramAngle = std::atan2(paramDir.y, paramDir.x);

            float centerWeight = 0.0f;
            if (!centerSamples.empty())
            {
                float maxDirRadius = 1.0f;
                for (const auto& s : directionalSamples)
                {
                    maxDirRadius = std::max(maxDirRadius, s.radius);
                }

                float move01 = std::clamp(paramMag / std::max(maxDirRadius, kBlendEpsilon), 0.0f, 1.0f);
                centerWeight = 1.0f - move01;
            }

            if (paramMag <= kBlendEpsilon || directionalSamples.empty())
            {
                if (!centerSamples.empty())
                {
                    float w = 1.0f / static_cast<float>(centerSamples.size());
                    for (const BlendTreeChild* child : centerSamples)
                    {
                        entries.push_back({ child->animationIndex, w, child->timeScale });
                    }
                }
                else if (!directionalSamples.empty())
                {
                    auto nearest = std::min_element(directionalSamples.begin(), directionalSamples.end(),
                        [&](const DirSample& a, const DirSample& b)
                        {
                            float da = std::abs(std::atan2(std::sin(paramAngle - a.angle), std::cos(paramAngle - a.angle)));
                            float db = std::abs(std::atan2(std::sin(paramAngle - b.angle), std::cos(paramAngle - b.angle)));
                            return da < db;
                        });

                    entries.push_back({ nearest->child->animationIndex, 1.0f, nearest->child->timeScale });
                }
            }
            else
            {
                std::sort(directionalSamples.begin(), directionalSamples.end(), [](const DirSample& a, const DirSample& b)
                    {
                        return a.angle < b.angle;
                    });

                int aIndex = 0;
                int bIndex = 0;
                bool foundSector = false;
                const size_t n = directionalSamples.size();
                for (size_t i = 0; i < n; ++i)
                {
                    const size_t j = (i + 1) % n;
                    float aAngle = directionalSamples[i].angle;
                    float bAngle = directionalSamples[j].angle;
                    if (j == 0) bAngle += XM_2PI;

                    float pAngle = paramAngle;
                    if (pAngle < aAngle) pAngle += XM_2PI;

                    if (pAngle >= aAngle && pAngle <= bAngle)
                    {
                        aIndex = static_cast<int>(i);
                        bIndex = static_cast<int>(j);
                        foundSector = true;
                        break;
                    }
                }

                if (!foundSector)
                {
                    auto nearest = std::min_element(directionalSamples.begin(), directionalSamples.end(),
                        [&](const DirSample& a, const DirSample& b)
                        {
                            float da = std::abs(std::atan2(std::sin(paramAngle - a.angle), std::cos(paramAngle - a.angle)));
                            float db = std::abs(std::atan2(std::sin(paramAngle - b.angle), std::cos(paramAngle - b.angle)));
                            return da < db;
                        });

                    entries.push_back({ nearest->child->animationIndex, 1.0f, nearest->child->timeScale });
                }
                else
                {
                    const DirSample& a = directionalSamples[aIndex];
                    const DirSample& b = directionalSamples[bIndex];

                    float aAngle = a.angle;
                    float bAngle = b.angle;
                    if (bIndex == 0 && bAngle < aAngle)
                    {
                        bAngle += XM_2PI;
                    }

                    float pAngle = paramAngle;
                    if (pAngle < aAngle) pAngle += XM_2PI;

                    float span = std::max(kBlendEpsilon, bAngle - aAngle);
                    float angleT = std::clamp((pAngle - aAngle) / span, 0.0f, 1.0f);

                    float radialA = 1.0f - std::abs(paramMag - a.radius) / std::max(a.radius, kBlendEpsilon);
                    float radialB = 1.0f - std::abs(paramMag - b.radius) / std::max(b.radius, kBlendEpsilon);
                    radialA = std::clamp(radialA, 0.0f, 1.0f);
                    radialB = std::clamp(radialB, 0.0f, 1.0f);

                    float dirAWeight = (1.0f - angleT) * radialA;
                    float dirBWeight = angleT * radialB;
                    float dirSum = std::max(kBlendEpsilon, dirAWeight + dirBWeight);
                    dirAWeight /= dirSum;
                    dirBWeight /= dirSum;

                    float directionalBudget = std::clamp(1.0f - centerWeight, 0.0f, 1.0f);
                    entries.push_back({ a.child->animationIndex, dirAWeight * directionalBudget, a.child->timeScale });
                    entries.push_back({ b.child->animationIndex, dirBWeight * directionalBudget, b.child->timeScale });
                }

                if (centerWeight > kBlendEpsilon && !centerSamples.empty())
                {
                    float each = centerWeight / static_cast<float>(centerSamples.size());
                    for (const BlendTreeChild* child : centerSamples)
                    {
                        entries.push_back({ child->animationIndex, each, child->timeScale });
                    }
                }
            }
        }
        else
        {
            float weightSum = 0.0f;
            for (const auto& child : blendTree->children)
            {
                float dist = (p - child.position).Length();
                float w = 1.0f / std::max(kBlendEpsilon, dist);
                entries.push_back({ child.animationIndex, w, child.timeScale });
                weightSum += w;
            }

            if (weightSum > 0.0f)
            {
                for (auto& e : entries)
                {
                    e.weight /= weightSum;
                }
            }
        }
    }

    std::vector<Model::Bone> accum = bones;
    bool hasAccum = false;
    float totalWeight = 0.0f;

    for (const auto& e : entries)
    {
        if (e.animIndex < 0 || e.weight <= 0.0f) continue;

        float clipLength = getAnimationLength(e.animIndex);
        if (clipLength <= 0.0f) continue;

        float normalized = 0.0f;
        float stateLength = getStateLength(state);
        if (stateLength > 0.0f)
        {
            normalized = std::clamp(time / stateLength, 0.0f, 1.0f);
        }
        float sampleTime = normalized * clipLength * std::max(0.001f, e.timeScale);
        sampleTime = std::fmod(sampleTime, std::max(clipLength, 0.001f));

        std::vector<Model::Bone> sample = bones;
        evaluateAnimation(e.animIndex, sampleTime, sample);

        if (!hasAccum)
        {
            accum = sample;
            totalWeight = std::max(0.0001f, e.weight);
            hasAccum = true;
            continue;
        }

        float blendT = e.weight / std::max(0.0001f, totalWeight + e.weight);
        blendBones(accum, sample, blendT, accum);
        totalWeight += e.weight;
    }

    if (hasAccum)
    {
        bones = std::move(accum);
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
    bool additiveAffectScale,
    bool additiveAffectTranslation,
    std::vector<Model::Bone>& out)
{
    const auto* bindBones = (m_model && m_model->getResource())
        ? &m_model->getResource()->getModelData().bones
        : nullptr;

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
            XMVECTOR q = XMQuaternionNormalize(XMQuaternionSlerp(rS, rL, weight));
            XMStoreFloat4(&out[idx].rotate, q);
            XMStoreFloat3(&out[idx].translate, XMVectorLerp(tS, tL, weight));
        }
        else // Additive
        {
            // Additive は bind pose 基準の差分を適用する
            XMVECTOR identityR = XMQuaternionIdentity();
            XMVECTOR identityT = XMVectorZero();
            XMVECTOR identityS = XMVectorSet(1, 1, 1, 0);

            XMVECTOR sRef = identityS;
            XMVECTOR rRef = identityR;
            XMVECTOR tRef = identityT;

            if (bindBones && idx < static_cast<int>(bindBones->size()))
            {
                sRef = XMLoadFloat3(&(*bindBones)[idx].scale);
                rRef = XMQuaternionNormalize(XMLoadFloat4(&(*bindBones)[idx].rotate));
                tRef = XMLoadFloat3(&(*bindBones)[idx].translate);
            }

            const XMVECTOR minScale = XMVectorReplicate(0.0001f);
            const XMVECTOR safeRefS = XMVectorMax(sRef, minScale);

            XMVECTOR deltaS = XMVectorDivide(sL, safeRefS);
            XMVECTOR deltaR = XMQuaternionMultiply(XMQuaternionInverse(rRef), rL);
            deltaR = XMQuaternionNormalize(deltaR);
            XMVECTOR deltaT = XMVectorSubtract(tL, tRef);

            XMVECTOR addS = additiveAffectScale
                ? XMVectorLerp(identityS, deltaS, weight)
                : identityS;
            XMVECTOR addR = XMQuaternionNormalize(XMQuaternionSlerp(identityR, deltaR, weight));
            XMVECTOR addT = additiveAffectTranslation
                ? XMVectorLerp(identityT, deltaT, weight)
                : identityT;

            XMStoreFloat3(&out[idx].scale, XMVectorMultiply(sS, addS));
            XMVECTOR qOut = XMQuaternionNormalize(XMQuaternionMultiply(rS, addR));
            XMStoreFloat4(&out[idx].rotate, qOut);
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

float AnimationStateMachine::getStateLength(const AnimationState& state) const
{
    const BlendTreeData* blendTree = state.getBlendTree();
    if (!blendTree || blendTree->children.empty())
    {
        return getAnimationLength(state.getAnimationIndex());
    }

    float maxLen = 0.0f;
    for (const auto& child : blendTree->children)
    {
        maxLen = std::max(maxLen, getAnimationLength(child.animationIndex));
    }
    return maxLen;
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

