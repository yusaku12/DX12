#include "pch.h"
#include "ScriptComponent.h"

ScriptComponent::SubscriptionToken ScriptComponent::subscribeEvent(const std::string& eventName, EventBus::Handler handler)
{
    SubscriptionToken token = EventBus::Instance().subscribe(eventName, std::move(handler));
    if (token != 0)
    {
        m_subscriptionTokens.push_back(token);
    }
    return token;
}

void ScriptComponent::unsubscribeEvent(SubscriptionToken& token)
{
    if (token == 0)
    {
        return;
    }

    EventBus::Instance().unsubscribe(token);
    m_subscriptionTokens.erase(
        std::remove(m_subscriptionTokens.begin(), m_subscriptionTokens.end(), token),
        m_subscriptionTokens.end());
    token = 0;
}

void ScriptComponent::clearEventSubscriptions()
{
    for (SubscriptionToken token : m_subscriptionTokens)
    {
        EventBus::Instance().unsubscribe(token);
    }
    m_subscriptionTokens.clear();
}

void ScriptComponent::publishEvent(const std::string& eventName) const
{
    publishEvent(eventName, std::any{});
}

void ScriptComponent::publishEvent(const std::string& eventName, std::any payload) const
{
    EventBus::Instance().publish(
        eventName,
        std::move(payload),
        gameObject() ? gameObject()->getInstanceId() : getInstanceId());
}

void ScriptComponent::onDestroy()
{
    clearEventSubscriptions();
}