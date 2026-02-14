#include "pch.h"

void InputManager::update()
{
    if (!m_windowFocused)
        return;

    //! フレーム開始時にホイールリセット
    m_prevMouseWheel = m_mouseWheel;
    m_mouseWheel = 0;

    updateKeyboard();
    updateMouse();
    updateBuffer();
}

void InputManager::setWindowFocused(bool focused)
{
    m_windowFocused = focused;

    if (!focused)
    {
        ZeroMemory(m_currKeys, 256);
        ZeroMemory(m_prevKeys, 256);
        ZeroMemory(m_currMouse, sizeof(m_currMouse));
        ZeroMemory(m_prevMouse, sizeof(m_prevMouse));
        m_mouseWheel = 0;
        m_prevMouseWheel = 0;
    }
}

void InputManager::addMouseWheel(int delta)
{
    m_mouseWheel += delta;
}

void InputManager::updateKeyboard()
{
    memcpy(m_prevKeys, m_currKeys, 256);
    GetKeyboardState(m_currKeys);
}

bool InputManager::isKeyPressed(uint8_t key) const
{
    return !(m_prevKeys[key] & 0x80) && (m_currKeys[key] & 0x80);
}

bool InputManager::isKeyHeld(uint8_t key) const
{
    return (m_currKeys[key] & 0x80);
}

bool InputManager::isKeyReleased(uint8_t key) const
{
    return (m_prevKeys[key] & 0x80) && !(m_currKeys[key] & 0x80);
}

void InputManager::updateMouse()
{
    memcpy(m_prevMouse, m_currMouse, sizeof(m_currMouse));

    m_currMouse[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    m_currMouse[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    m_currMouse[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

    m_prevMousePos = m_mousePos;
    GetCursorPos(&m_mousePos);

    m_mouseDelta.x = m_mousePos.x - m_prevMousePos.x;
    m_mouseDelta.y = m_mousePos.y - m_prevMousePos.y;
}

void InputManager::updateBuffer()
{
    const float dt = TimeManager::Instance().getDeltaTime();

    //! Pressed 検出 → Buffer 登録
    for (auto& [name, action] : m_actionBindings)
    {
        for (uint8_t key : action.keys)
        {
            if (isKeyPressed(key))
            {
                auto& buf = m_actionBuffers[name];
                buf.timeLeft = action.bufferTime;
                buf.triggered = true;
                break;
            }
        }
    }

    //! Buffer 寿命管理
    for (auto it = m_actionBuffers.begin(); it != m_actionBuffers.end(); )
    {
        it->second.timeLeft -= dt;
        if (it->second.timeLeft <= 0.0f)
            it = m_actionBuffers.erase(it);
        else
            ++it;
    }
}

bool InputManager::isMousePressed(uint8_t button) const
{
    return !m_prevMouse[button] && m_currMouse[button];
}

bool InputManager::isMouseHeld(uint8_t button) const
{
    return m_currMouse[button];
}

bool InputManager::isMouseReleased(uint8_t button) const
{
    return m_prevMouse[button] && !m_currMouse[button];
}

void InputManager::bindAction(const std::string& actionName, uint8_t key, float bufferTime)
{
    auto& action = m_actionBindings[actionName];
    action.keys.push_back(key);

    if (bufferTime >= 0.0f)
        action.bufferTime = bufferTime;
}

InputState InputManager::getActionState(const std::string& actionName) const
{
    auto it = m_actionBindings.find(actionName);
    if (it == m_actionBindings.end())
        return InputState::None;

    const auto& action = it->second;

    for (uint8_t key : action.keys)
    {
        if (isKeyPressed(key))  return InputState::Pressed;
        if (isKeyHeld(key))     return InputState::Held;
        if (isKeyReleased(key)) return InputState::Released;
    }

    return InputState::None;
}

void InputManager::bindAxis(const std::string& name, uint8_t negative, uint8_t positive)
{
    m_axes[name] = { negative, positive };
}

float InputManager::getAxis(const std::string& name) const
{
    auto it = m_axes.find(name);
    if (it == m_axes.end())
        return 0.0f;

    float v = 0.0f;
    if (isKeyHeld(it->second.negativeKey)) v -= 1.0f;
    if (isKeyHeld(it->second.positiveKey)) v += 1.0f;

    return v;
}

bool InputManager::consumeAction(const std::string& actionName)
{
    auto it = m_actionBuffers.find(actionName);
    if (it == m_actionBuffers.end())
        return false;

    m_actionBuffers.erase(it);
    return true;
}