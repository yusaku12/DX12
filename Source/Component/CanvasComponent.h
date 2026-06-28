#pragma once

#include "Component.h"

class CanvasComponent : public Component
{
public:

    void onEnable() override;
    void onDisable() override;
    void onDestroy() override;
    void inspectGUI() override;

    int getSortOrder() const { return m_sortOrder; }
    void setSortOrder(int value) { m_sortOrder = value; }

    bool receivesInput() const { return m_receivesInput; }
    void setReceivesInput(bool value) { m_receivesInput = value; }

private:

    int m_sortOrder = 0;
    bool m_receivesInput = true;
};