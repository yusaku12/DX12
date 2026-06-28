#pragma once
#include "CpuGpuProfiler.h"

//=====================================================
// 時間管理を行うクラス
//=====================================================
class TimeManager
{
public:

    //! インスタンス取得
    static TimeManager& Instance()
    {
        static TimeManager instance;
        return instance;
    }

    //! 初期化
    void initialize();

    //! 更新処理
    void update();

    //! フレーム開始処理
    void frameStart(ID3D12GraphicsCommandList* cmd);

    // ! フレーム終了処理
    void frameEnd(ID3D12GraphicsCommandList* cmd);

    //! imgui
    void imgui();

    //! profiler中身表示
    void renderProfilerContents();

    //! 再生状態制御
    void play();
    void pause();
    void setPaused(bool paused);
    void togglePause();
    bool isPaused() const { return m_pause; }

    //! 1フレームだけ進める
    void requestSingleStep();

    //! 時間スケール取得・設定
    float getTimeScale() const { return m_timeScale; }
    void setTimeScale(float value);

    //! 経過時間取得 (スケール有り)
    float getDeltaTime() const { return m_deltaTime; }

    //! FPS取得
    int getFPS() const { return m_fps; }

private:

    TimeManager() = default;
    ~TimeManager() = default;

    //! FPS 計算
    void calculateFPS();

    bool m_initialized = false; //!< 初期化済みフラグ
    bool m_pause = false; //!< 一時停止フラグ
    std::chrono::high_resolution_clock::time_point m_lastTime;  //!< 前回時間
    std::chrono::high_resolution_clock::time_point m_startTime; //!< 開始時間
    float m_deltaTime = 0.0f; //!< デルタタイム
    float m_unscaledDeltaTime = 0.0f; //!< スケール無しデルタタイム
    float m_time = 0.0f; //!< 経過時間
    float m_unscaledTime = 0.0f; //!< スケール無し経過時間
    float m_smoothDeltaTime = 0.0f; //!< 平滑化デルタタイム
    float m_timeScale = 1.0f; //!< 時間スケール
    bool m_stepRequested = false; //!< 一時停止中の1フレーム実行要求
    float m_stepDuration = 1.0f / 60.0f; //!< step 時の固定デルタタイム
    int   m_fps = 0; //!< FPS
    float m_fpsTimer = 0.0f; //!< FPS 計測タイマー
    int   m_fpsFrameCounter = 0; //!< FPS フレームカウンター
    CpuGpuProfiler m_profiler; //!< CPU/GPU プロファイラ
};