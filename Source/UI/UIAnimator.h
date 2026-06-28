#pragma once

//=====================================================
//! イーズ関数の種類
//=====================================================
enum class UIEaseType : uint8_t
{
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInBack,
    EaseOutBack,
    EaseInOutBack,
    Spring,     //!< バウンス付きオーバーシュート
    Max
};

//=====================================================
//! アニメーショントランジションシステム（Tween）
//!
//! UI ウィジェットの position / size / color / alpha を
//! 指定イーズ関数で滑らかに遷移させる。
//! 遅延・シーケンス・ループをサポートする。
//=====================================================
class UIAnimator
{
public:

    static UIAnimator& Instance()
    {
        static UIAnimator instance;
        return instance;
    }

    //! フレーム更新（RuntimeUIManager::update() 内で呼ぶ）
    void update(float deltaTime);

    //! 全 Tween をクリア
    void clear();

    // ── Tween 生成 API ──────────────────────────────

    //! float 値をアニメーション
    //! @param target      対象 float へのポインタ
    //! @param to          目標値
    //! @param duration    秒数
    //! @param ease        イーズ種別
    //! @param delay       開始遅延（秒）
    //! @param onComplete  完了コールバック（省略可）
    //! @return  Tween ID（キャンセルに使用）
    uint64_t animateFloat(float* target, float to, float duration,
                          UIEaseType ease   = UIEaseType::EaseOutQuad,
                          float      delay  = 0.f,
                          std::function<void()> onComplete = nullptr);

    //! Vector2 値をアニメーション（position / size 用）
    uint64_t animateVector2(Vector2* target, const Vector2& to, float duration,
                            UIEaseType ease   = UIEaseType::EaseOutQuad,
                            float      delay  = 0.f,
                            std::function<void()> onComplete = nullptr);

    //! Vector4 値をアニメーション（color 用）
    uint64_t animateVector4(Vector4* target, const Vector4& to, float duration,
                            UIEaseType ease   = UIEaseType::EaseOutQuad,
                            float      delay  = 0.f,
                            std::function<void()> onComplete = nullptr);

    //! 指定 ID の Tween をキャンセル（即座に目標値に設定しない）
    void cancel(uint64_t id);

    //! 指定ポインタに紐付いた Tween を全てキャンセル
    void cancelAll(const void* target);

    //! イーズ関数を直接評価する（t: [0, 1] → output: [0, 1]）
    static float evaluate(UIEaseType ease, float t);

private:

    UIAnimator() = default;
    ~UIAnimator() = default;
    UIAnimator(const UIAnimator&) = delete;
    UIAnimator& operator=(const UIAnimator&) = delete;

    //! Tween の種類
    enum class TweenType : uint8_t { Float, Vector2, Vector4 };

    //! Tween エントリ
    struct Tween
    {
        uint64_t id       = 0;
        TweenType type    = TweenType::Float;
        UIEaseType ease   = UIEaseType::Linear;
        float elapsed     = 0.f;
        float delay       = 0.f;
        float duration    = 1.f;
        bool  done        = false;

        void*    target = nullptr;   //!< 対象データへのポインタ

        // 開始値・目標値（最大 4 成分）
        float from[4] = {};
        float to[4]   = {};

        std::function<void()> onComplete;
    };

    std::vector<Tween> m_tweens;
    uint64_t           m_nextId = 1;
};
