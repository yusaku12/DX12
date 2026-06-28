#pragma once
#include <Xinput.h>

//=====================================================
//! 入力の状態を表す列挙型
//=====================================================
enum class InputState
{
    None,
    Pressed,   //!< 今フレームで押された
    Held,      //!< 押し続けている
    Released,  //!< 今フレームで離された
    Max
};

//=====================================================
//! バッファード入力の状態を表す列挙型
//=====================================================
enum class BufferedState
{
    None,
    Buffered,
    Consumed
};

//=====================================================
//! ゲームパッドのボタン列挙型
//!
//! 下位 16 ビットは XINPUT_GAMEPAD_* と互換。
//! LeftTrigger / RightTrigger はアナログトリガーの
//! デジタル押し込み判定用拡張ビット（閾値超過で ON）。
//=====================================================
enum class GamepadButton : uint32_t
{
    DPadUp        = 0x00000001,   //!< 十字キー 上
    DPadDown      = 0x00000002,   //!< 十字キー 下
    DPadLeft      = 0x00000004,   //!< 十字キー 左
    DPadRight     = 0x00000008,   //!< 十字キー 右
    Start         = 0x00000010,   //!< スタートボタン
    Back          = 0x00000020,   //!< バックボタン (Select)
    LeftThumb     = 0x00000040,   //!< 左スティック押し込み
    RightThumb    = 0x00000080,   //!< 右スティック押し込み
    LeftShoulder  = 0x00000100,   //!< LB ボタン
    RightShoulder = 0x00000200,   //!< RB ボタン
    A             = 0x00001000,   //!< A ボタン
    B             = 0x00002000,   //!< B ボタン
    X             = 0x00004000,   //!< X ボタン
    Y             = 0x00008000,   //!< Y ボタン
    LeftTrigger   = 0x00010000,   //!< LT デジタル押し込み（閾値超過）
    RightTrigger  = 0x00020000,   //!< RT デジタル押し込み（閾値超過）
};

//=====================================================
//! ゲームパッドの軸入力列挙型
//=====================================================
enum class GamepadAxis : uint8_t
{
    LeftStickX,    //!< 左スティック 水平軸  [-1, 1]
    LeftStickY,    //!< 左スティック 垂直軸  [-1, 1]  (+が上方向)
    RightStickX,   //!< 右スティック 水平軸  [-1, 1]
    RightStickY,   //!< 右スティック 垂直軸  [-1, 1]  (+が上方向)
    LeftTrigger,   //!< 左トリガー アナログ値 [0, 1]
    RightTrigger,  //!< 右トリガー アナログ値 [0, 1]
};

//=====================================================
//! インプットマネージャー
//!
//! キーボード・マウス・XInput ゲームパッド（最大 4 台）の
//! 入力を一元管理するシングルトン。
//=====================================================
class InputManager
{
public:

    //! シングルトンインスタンス取得
    static InputManager& Instance()
    {
        static InputManager instance;
        return instance;
    }

    ~InputManager();

    //! 毎フレーム更新（フレーム先頭で呼ぶこと）
    void update();

    //! ウィンドウフォーカス変化時に呼ぶ
    void setWindowFocused(bool focused);

    //! WM_MOUSEWHEEL ハンドラから呼ぶ
    void addMouseWheel(int delta);

    // =========================================================
    //  キーボード
    // =========================================================

    //! キーが今フレームで押されたか
    bool isKeyPressed(uint8_t key) const;

    //! キーが押し続けられているか
    bool isKeyHeld(uint8_t key) const;

    //! キーが今フレームで離されたか
    bool isKeyReleased(uint8_t key) const;

    // =========================================================
    //  マウス
    // =========================================================

    //! マウスボタンが今フレームで押されたか  (0=左 1=右 2=中)
    bool isMousePressed(uint8_t button) const;

    //! マウスボタンが押し続けられているか
    bool isMouseHeld(uint8_t button) const;

    //! マウスボタンが今フレームで離されたか
    bool isMouseReleased(uint8_t button) const;

    //! マウス位置取得（スクリーン座標）
    POINT getMousePosition() const { return m_mousePos; }

    //! 前フレームからのマウス移動量取得
    POINT getMouseDelta() const { return m_mouseDelta; }

    //! マウスホイールの前フレームデルタ取得
    int getMouseWheel() const { return m_prevMouseWheel; }

    // =========================================================
    //  ゲームパッド (XInput)
    // =========================================================

    //! コントローラーが接続されているか
    //! @param index  コントローラーインデックス [0, 3]
    bool isGamepadConnected(int index = 0) const;

    //! 接続中のコントローラー数を返す
    int getConnectedGamepadCount() const;

    //! ゲームパッドボタンが今フレームで押されたか
    //! @param btn    ボタン種別
    //! @param index  コントローラーインデックス（-1 = 接続中の全コントローラー）
    bool isGamepadButtonPressed(GamepadButton btn, int index = 0) const;

    //! ゲームパッドボタンが押し続けられているか
    bool isGamepadButtonHeld(GamepadButton btn, int index = 0) const;

    //! ゲームパッドボタンが今フレームで離されたか
    bool isGamepadButtonReleased(GamepadButton btn, int index = 0) const;

    //! ゲームパッドの軸値を取得
    //! スティック: [-1, 1]、トリガー: [0, 1]
    //! @param index  コントローラーインデックス（-1 = 最大絶対値を返す）
    float getGamepadAxis(GamepadAxis axis, int index = 0) const;

    //! バイブレーションを設定する
    //! @param leftMotor   左モーター強度 [0, 1]
    //! @param rightMotor  右モーター強度 [0, 1]
    //! @param duration    持続時間（秒）。-1.0f で無限継続
    //! @param index       コントローラーインデックス [0, 3]
    void setGamepadVibration(float leftMotor, float rightMotor,
                             float duration = -1.0f, int index = 0);

    //! 指定コントローラーのバイブレーションを停止する
    void stopGamepadVibration(int index = 0);

    //! 全コントローラーのバイブレーションを停止する
    void stopAllGamepadVibration();

    // =========================================================
    //  アクション / 軸 マッピング
    // =========================================================

    //! キーボードキーをアクションにバインドする
    //! 同一アクションに複数回呼ぶことで複数キーをバインド可能
    void bindAction(const std::string& actionName, uint8_t key,
                    float bufferTime = 0.15f);

    //! ゲームパッドボタンをアクションにバインドする
    //! @param controllerIndex  -1 = 接続中の全コントローラー
    void bindAction(const std::string& actionName, GamepadButton button,
                    float bufferTime = 0.15f, int controllerIndex = -1);

    //! アクション状態取得（キーボード + ゲームパッド両対応）
    InputState getActionState(const std::string& actionName) const;

    //! キーボードキーを軸にバインドする
    void bindAxis(const std::string& name, uint8_t negative, uint8_t positive);

    //! ゲームパッド軸を軸にバインドする
    //! @param controllerIndex  -1 = 接続中の全コントローラー（最大絶対値を採用）
    //! @param invert           軸の向きを反転するか
    void bindAxis(const std::string& name, GamepadAxis axis,
                  int controllerIndex = -1, bool invert = false);

    //! 軸入力値取得（キーボード + ゲームパッド の絶対値最大を採用）
    float getAxis(const std::string& name) const;

    //! バッファード入力を消費する（True なら今フレームにバッファあり）
    bool consumeAction(const std::string& actionName);

private:

    InputManager() = default;

    // ---------------------------------------------------------
    //  内部構造体
    // ---------------------------------------------------------

    //! ゲームパッドボタンのバインディングエントリ
    struct GamepadButtonBind
    {
        GamepadButton button;
        int           controllerIndex = -1;   //!< -1 = 全コントローラー
    };

    //! アクションバインディング
    struct ActionBinding
    {
        std::vector<uint8_t>           keys;
        std::vector<GamepadButtonBind> gamepadButtons;
        float                          bufferTime = 0.15f;
    };

    //! バッファード入力エントリ
    struct InputBufferEntry
    {
        float timeLeft  = 0.0f;
        bool  triggered = false;
    };

    //! 入力軸
    struct InputAxis
    {
        uint8_t     negativeKey    = 0;
        uint8_t     positiveKey    = 0;
        bool        hasGamepadAxis = false;
        GamepadAxis gpAxis         = GamepadAxis::LeftStickX;
        int         gpIndex        = -1;     //!< -1 = 全コントローラー
        bool        gpInvert       = false;
    };

    //! コントローラー 1 台分の状態
    struct GamepadState
    {
        bool     connected         = false;
        uint32_t prevButtons       = 0;    //!< 前フレームのボタンビットフィールド
        uint32_t currButtons       = 0;    //!< 今フレームのボタンビットフィールド
        float    leftStickX        = 0.f;
        float    leftStickY        = 0.f;
        float    rightStickX       = 0.f;
        float    rightStickY       = 0.f;
        float    leftTrigger       = 0.f;  //!< アナログ値 [0, 1]
        float    rightTrigger      = 0.f;  //!< アナログ値 [0, 1]
        float    vibrationDuration = 0.f;  //!< 残り秒数（-1.0f = 無限）
        float    leftMotor         = 0.f;
        float    rightMotor        = 0.f;
    };

    // ---------------------------------------------------------
    //  更新処理
    // ---------------------------------------------------------

    void updateKeyboard();
    void updateMouse();
    void updateGamepads();
    void updateVibration(float dt);
    void updateBuffer();

    // ---------------------------------------------------------
    //  ヘルパー
    // ---------------------------------------------------------

    //! スティック用の放射状デッドゾーン処理
    static void applyStickDeadzone(float rawX, float rawY, float deadzone,
                                   float& outX, float& outY);

    //! トリガー用のデッドゾーン処理 (0–255 → [0, 1])
    static float applyTriggerDeadzone(uint8_t raw);

    //! インデックス範囲チェック [0, XUSER_MAX_COUNT)
    static bool isValidIndex(int index);

    //! 単一コントローラーのボタン判定（内部用）
    bool checkGamepadButtonPressed (GamepadButton btn, int index) const;
    bool checkGamepadButtonHeld    (GamepadButton btn, int index) const;
    bool checkGamepadButtonReleased(GamepadButton btn, int index) const;

    //! 単一コントローラーの軸取得（内部用）
    float getGamepadAxisInternal(GamepadAxis axis, int index) const;

    //! XINPUT_GAMEPAD からボタンビットフィールドを構築する
    static uint32_t buildButtonMask(const XINPUT_GAMEPAD& gp);

    //! バイブレーション値を XInput に送信する
    void applyVibration(int index);

    // ---------------------------------------------------------
    //  キーボード / マウス
    // ---------------------------------------------------------

    uint8_t m_prevKeys[256]{};
    uint8_t m_currKeys[256]{};

    uint8_t m_prevMouse[3]{};
    uint8_t m_currMouse[3]{};

    POINT m_mousePos{};
    POINT m_prevMousePos{};
    POINT m_mouseDelta{};

    int m_mouseWheel     = 0;
    int m_prevMouseWheel = 0;

    // ---------------------------------------------------------
    //  ゲームパッド
    // ---------------------------------------------------------

    GamepadState m_gamepads[XUSER_MAX_COUNT]{};

    //! 未接続コントローラーのポーリング間隔カウンタ
    int m_disconnectedPollCounter = 0;

    //! 未接続コントローラーは N フレームに 1 回だけポーリングする
    static constexpr int k_disconnectedPollInterval = 60;

    //! トリガーのデジタル判定閾値（XInput 推奨: 30 / 255）
    static constexpr uint8_t k_triggerThreshold = 30;

    //! 左スティックのデッドゾーン（XInput 推奨値: 7849 / 32767）
    static constexpr float k_leftStickDeadzone  = 7849.f;

    //! 右スティックのデッドゾーン（XInput 推奨値: 8689 / 32767）
    static constexpr float k_rightStickDeadzone = 8689.f;

    // ---------------------------------------------------------
    //  アクション / 軸
    // ---------------------------------------------------------

    std::unordered_map<std::string, InputAxis>        m_axes;
    std::unordered_map<std::string, ActionBinding>    m_actionBindings;
    std::unordered_map<std::string, InputBufferEntry> m_actionBuffers;

    bool m_windowFocused = true;
};