#pragma once

namespace GpuDebugMarker
{
    inline void begin(ID3D12GraphicsCommandList* cmd, const char* label)
    {
        (void)cmd;
        (void)label;
    }

    inline void end(ID3D12GraphicsCommandList* cmd)
    {
        (void)cmd;
    }

    inline void mark(ID3D12GraphicsCommandList* cmd, const char* label)
    {
        (void)cmd;
        (void)label;
    }

    class ScopedEvent
    {
    public:
        ScopedEvent(ID3D12GraphicsCommandList* cmd, const char* label)
            : m_cmd(cmd)
            , m_active(false)
        {
            if (m_cmd && label && label[0] != '\0')
            {
                begin(m_cmd, label);
                m_active = true;
            }
        }

        ~ScopedEvent()
        {
            if (m_active)
            {
                end(m_cmd);
            }
        }

        ScopedEvent(const ScopedEvent&) = delete;
        ScopedEvent& operator=(const ScopedEvent&) = delete;

    private:
        ID3D12GraphicsCommandList* m_cmd;
        bool m_active;
    };
}
