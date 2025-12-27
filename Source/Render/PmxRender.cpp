#include "pch.h"
#include "PmxRender.h"

void PmxRender::render() const
{
    for (const auto& actor : m_pmxActor)
    {
        actor->render();
    }
}

void PmxRender::addPmxActor(const std::shared_ptr<PmxActor>& pmxActor)
{
    m_pmxActor.emplace_back(pmxActor);
}