#include "pch.h"
#include "UIAnimator.h"

// =============================================================
//  イーズ関数（t: [0,1] → [0,1]）
// =============================================================
float UIAnimator::evaluate(UIEaseType ease, float t)
{
    t = std::clamp(t, 0.f, 1.f);

    switch (ease)
    {
    case UIEaseType::Linear:
        return t;

    case UIEaseType::EaseInQuad:
        return t * t;

    case UIEaseType::EaseOutQuad:
        return t * (2.f - t);

    case UIEaseType::EaseInOutQuad:
        return (t < 0.5f) ? 2.f * t * t : -1.f + (4.f - 2.f * t) * t;

    case UIEaseType::EaseInCubic:
        return t * t * t;

    case UIEaseType::EaseOutCubic:
    {
        const float f = t - 1.f;
        return f * f * f + 1.f;
    }

    case UIEaseType::EaseInOutCubic:
        return (t < 0.5f)
            ? 4.f * t * t * t
            : (t - 1.f) * (2.f * t - 2.f) * (2.f * t - 2.f) + 1.f;

    case UIEaseType::EaseInBack:
    {
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.f;
        return c3 * t * t * t - c1 * t * t;
    }

    case UIEaseType::EaseOutBack:
    {
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.f;
        const float u = t - 1.f;
        return 1.f + c3 * u * u * u + c1 * u * u;
    }

    case UIEaseType::EaseInOutBack:
    {
        constexpr float c1 = 1.70158f;
        constexpr float c2 = c1 * 1.525f;
        if (t < 0.5f)
        {
            const float u = 2.f * t;
            return (u * u * ((c2 + 1.f) * u - c2)) * 0.5f;
        }
        const float u = 2.f * t - 2.f;
        return (u * u * ((c2 + 1.f) * u + c2) + 2.f) * 0.5f;
    }

    case UIEaseType::Spring:
    {
        //! 減衰振動によるバウンス
        constexpr float omega = 20.f;
        constexpr float zeta  = 0.5f;
        const float e = std::expf(-zeta * omega * t);
        return 1.f - e * std::cosf(std::sqrtf(1.f - zeta * zeta) * omega * t);
    }

    default:
        return t;
    }
}

// =============================================================
//  更新
// =============================================================
void UIAnimator::update(float deltaTime)
{
    for (auto& tw : m_tweens)
    {
        if (tw.done) continue;

        // 遅延処理
        if (tw.delay > 0.f)
        {
            tw.delay -= deltaTime;
            if (tw.delay > 0.f) continue;
            deltaTime = -tw.delay; // 遅延を超えた分だけ elapsed に加算
            tw.delay  = 0.f;
        }

        tw.elapsed += deltaTime;
        const float rawT = (tw.duration > 0.f)
            ? std::clamp(tw.elapsed / tw.duration, 0.f, 1.f)
            : 1.f;
        const float easedT = evaluate(tw.ease, rawT);

        switch (tw.type)
        {
        case TweenType::Float:
        {
            auto* p = static_cast<float*>(tw.target);
            *p = tw.from[0] + (tw.to[0] - tw.from[0]) * easedT;
            break;
        }
        case TweenType::Vector2:
        {
            auto* p = static_cast<Vector2*>(tw.target);
            p->x = tw.from[0] + (tw.to[0] - tw.from[0]) * easedT;
            p->y = tw.from[1] + (tw.to[1] - tw.from[1]) * easedT;
            break;
        }
        case TweenType::Vector4:
        {
            auto* p = static_cast<Vector4*>(tw.target);
            p->x = tw.from[0] + (tw.to[0] - tw.from[0]) * easedT;
            p->y = tw.from[1] + (tw.to[1] - tw.from[1]) * easedT;
            p->z = tw.from[2] + (tw.to[2] - tw.from[2]) * easedT;
            p->w = tw.from[3] + (tw.to[3] - tw.from[3]) * easedT;
            break;
        }
        }

        if (rawT >= 1.f)
        {
            tw.done = true;
            if (tw.onComplete) tw.onComplete();
        }
    }

    // 完了分を一括削除
    m_tweens.erase(
        std::remove_if(m_tweens.begin(), m_tweens.end(),
                       [](const Tween& t) { return t.done; }),
        m_tweens.end());
}

void UIAnimator::clear()
{
    m_tweens.clear();
}

// =============================================================
//  Tween 生成
// =============================================================
uint64_t UIAnimator::animateFloat(float* target, float to, float duration,
                                  UIEaseType ease, float delay,
                                  std::function<void()> onComplete)
{
    cancelAll(target); // 既存の同一ターゲット Tween をキャンセル

    Tween tw{};
    tw.id         = m_nextId++;
    tw.type       = TweenType::Float;
    tw.ease       = ease;
    tw.duration   = std::max(0.001f, duration);
    tw.delay      = std::max(0.f, delay);
    tw.target     = target;
    tw.from[0]    = *target;
    tw.to[0]      = to;
    tw.onComplete = std::move(onComplete);
    m_tweens.push_back(std::move(tw));

    return m_tweens.back().id;
}

uint64_t UIAnimator::animateVector2(Vector2* target, const Vector2& to,
                                    float duration, UIEaseType ease, float delay,
                                    std::function<void()> onComplete)
{
    cancelAll(target);

    Tween tw{};
    tw.id         = m_nextId++;
    tw.type       = TweenType::Vector2;
    tw.ease       = ease;
    tw.duration   = std::max(0.001f, duration);
    tw.delay      = std::max(0.f, delay);
    tw.target     = target;
    tw.from[0]    = target->x;  tw.from[1] = target->y;
    tw.to[0]      = to.x;       tw.to[1]   = to.y;
    tw.onComplete = std::move(onComplete);
    m_tweens.push_back(std::move(tw));

    return m_tweens.back().id;
}

uint64_t UIAnimator::animateVector4(Vector4* target, const Vector4& to,
                                    float duration, UIEaseType ease, float delay,
                                    std::function<void()> onComplete)
{
    cancelAll(target);

    Tween tw{};
    tw.id         = m_nextId++;
    tw.type       = TweenType::Vector4;
    tw.ease       = ease;
    tw.duration   = std::max(0.001f, duration);
    tw.delay      = std::max(0.f, delay);
    tw.target     = target;
    tw.from[0] = target->x; tw.from[1] = target->y;
    tw.from[2] = target->z; tw.from[3] = target->w;
    tw.to[0] = to.x; tw.to[1] = to.y;
    tw.to[2] = to.z; tw.to[3] = to.w;
    tw.onComplete = std::move(onComplete);
    m_tweens.push_back(std::move(tw));

    return m_tweens.back().id;
}

void UIAnimator::cancel(uint64_t id)
{
    for (auto& tw : m_tweens)
    {
        if (tw.id == id)
        {
            tw.done = true;
            return;
        }
    }
}

void UIAnimator::cancelAll(const void* target)
{
    for (auto& tw : m_tweens)
    {
        if (tw.target == target)
            tw.done = true;
    }
}
