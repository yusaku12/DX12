#pragma once

#include "Component.h"
#include "System\EventBus.h"

//=====================================================
// Script-friendly component base with event subscription helpers.
//=====================================================
class ScriptComponent : public Component
{
public:

    ~ScriptComponent() override = default;

    using Event = EventBus::Event;
    using SubscriptionToken = EventBus::SubscriptionToken;

    SubscriptionToken subscribeEvent(const std::string& eventName, EventBus::Handler handler);
    void unsubscribeEvent(SubscriptionToken& token);
    void clearEventSubscriptions();

    void publishEvent(const std::string& eventName) const;
    void publishEvent(const std::string& eventName, std::any payload) const;

    void onDestroy() override;

protected:

    virtual void onEvent(const Event&) {}

    SubscriptionToken subscribeEvent(const std::string& eventName)
    {
        return subscribeEvent(
            eventName,
            [this](const Event& eventData)
            {
                onEvent(eventData);
            });
    }

private:

    std::vector<SubscriptionToken> m_subscriptionTokens;
};