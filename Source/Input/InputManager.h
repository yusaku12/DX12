#pragma once

//! 入力の状態を表す列挙型
enum class InputState
{
    None,
    Pressed,   //!< 今フレームで押された
    Held,      //!< 押し続けている
    Released,  //!< 今フレームで離された
    Max
};

//! バッファード入力の状態を表す列挙型
enum class BufferedState
{
    None,
    Buffered,
    Consumed
};

//=====================================================
// インプットマネージャークラス
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

    //! 更新
    void update();

    //! ウィンドウフォーカス設定
    void setWindowFocused(bool focused);

    //! キーが押されたか
    bool isKeyPressed(uint8_t key) const;

    //! キーが押されているか
    bool isKeyHeld(uint8_t key) const;

    //! キーが離されたか
    bool isKeyReleased(uint8_t key) const;

    //! マウスボタンが押されたか
    bool isMousePressed(uint8_t button) const;

    //! マウスボタンが押されているか
    bool isMouseHeld(uint8_t button) const;

    //! マウスボタンが離されたか
    bool isMouseReleased(uint8_t button) const;

    //! マウス位置取得
    POINT getMousePosition() const { return m_mousePos; }

    //! マウス移動量取得
    POINT getMouseDelta() const { return m_mouseDelta; }

    //! マウスホイール取得
    int getMouseWheel() const { return m_mouseWheel; }

    //! アクションマッピング
    void bindAction(const std::string& actionName, uint8_t key, float bufferTime);

    //! アクション状態取得
    InputState getActionState(const std::string& actionName) const;

    //! 軸入力バインド
    void bindAxis(const std::string& name, uint8_t negative, uint8_t positive);

    //! 軸入力値取得
    float getAxis(const std::string& name) const;

    //! バッファード入力状態取得
    bool consumeAction(const std::string& actionName);

private:

    InputManager() = default;

    //! 入力軸構造体
    struct InputAxis
    {
        uint8_t negativeKey;
        uint8_t positiveKey;
        float   value = 0.0f;
    };

    //! バッファード入力構造体
    struct InputBufferEntry
    {
        float timeLeft = 0.0f;
        bool  triggered = false;
    };

    //! アクションバインディング構造体
    struct ActionBinding
    {
        std::vector<uint8_t> keys;
        float bufferTime = 0.15f;   //!< アクション固有
    };

    //! ImGui入力ブロック更新
    void updateImGuiBlock();

    //! キーボード更新
    void updateKeyboard();

    //! マウス更新
    void updateMouse();

    //! バッファード入力更新
    void updateBuffer();

    //! キー状態
    uint8_t m_prevKeys[256]{};
    uint8_t m_currKeys[256]{};

    //! マウス状態
    uint8_t m_prevMouse[3]{};
    uint8_t m_currMouse[3]{};

    //! マウス位置・移動量
    POINT m_mousePos{};
    POINT m_prevMousePos{};
    POINT m_mouseDelta{};

    //! マウスホイール
    int m_mouseWheel = 0;

    //! 軸入力
    std::unordered_map<std::string, InputAxis> m_axes;

    //! Action 定義
    std::unordered_map<std::string, ActionBinding> m_actionBindings;

    //! Buffer
    std::unordered_map<std::string, InputBufferEntry> m_actionBuffers;

    //! 入力ブロックフラグ
    bool m_blockKeyboard = false;
    bool m_blockMouse = false;

    //! ウィンドウフォーカス状態
    bool m_windowFocused = true;
};