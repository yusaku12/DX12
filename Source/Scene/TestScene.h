#pragma once

#include "Scene.h"
#include <chrono>

//============================================================
// テスト用シーン
//============================================================
class TestScene : public Scene
{
public:

    void onEnter() override;

    void onExit() override;

    void update() override;

    void draw() override;

    void drawMultiThreaded() override;

    void debugDraw() override;

private:

    //! マルチスレッドデバッグ計測用
    struct ThreadTimingInfo
    {
        const char* name = "";
        float startMs = 0.0f;    //! フレーム開始からの相対時刻 (ms)
        float durationMs = 0.0f; //! 実行時間 (ms)
        std::thread::id threadId{};
    };

    std::chrono::high_resolution_clock::time_point m_frameStart{};
    std::vector<ThreadTimingInfo> m_threadTimings;
    float m_totalMultiThreadMs = 0.0f;
    float m_singleThreadEstimateMs = 0.0f;
    std::mutex m_timingMutex;
};