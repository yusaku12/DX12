#pragma once

namespace GpuDebugMarker
{
    inline void begin(ID3D12GraphicsCommandList* cmd, const char* label)
    {
        if (!cmd || !label || label[0] == '\0')
        {
            return;
        }

        cmd->BeginEvent(0, label, static_cast<UINT>(std::strlen(label)));
    }

    inline void end(ID3D12GraphicsCommandList* cmd)
    {
        if (!cmd)
        {
            return;
        }

        cmd->EndEvent();
    }

    inline void mark(ID3D12GraphicsCommandList* cmd, const char* label)
    {
        if (!cmd || !label || label[0] == '\0')
        {
            return;
        }

        cmd->SetMarker(0, label, static_cast<UINT>(std::strlen(label)));
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
