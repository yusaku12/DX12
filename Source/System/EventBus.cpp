#include "pch.h"
#include "EventBus.h"

EventBus::SubscriptionToken EventBus::subscribe(const std::string& eventName, Handler handler)
{
    if (eventName.empty() || !handler)
    {
        return 0;
    }

    SubscriptionToken token = m_nextToken++;
    m_subscriptions[eventName].push_back(Subscription{ token, std::move(handler) });
    return token;
}

void EventBus::unsubscribe(SubscriptionToken token)
{
    if (token == 0)
    {
        return;
    }

    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end();)
    {
        auto& listeners = it->second;
        listeners.erase(
            std::remove_if(
                listeners.begin(),
                listeners.end(),
                [token](const Subscription& subscription)
                {
                    return subscription.token == token;
                }),
            listeners.end());

        if (listeners.empty())
        {
            it = m_subscriptions.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void EventBus::publish(std::string eventName, uint64_t senderId)
{
    publish(std::move(eventName), std::any{}, senderId);
}

void EventBus::publish(std::string eventName, std::any payload, uint64_t senderId)
{
    if (eventName.empty())
    {
        return;
    }

    m_pendingEvents.push_back(Event{ std::move(eventName), senderId, std::move(payload) });
}

void EventBus::dispatchQueued()
{
    if (m_pendingEvents.empty())
    {
        return;
    }

    std::vector<Event> dispatchQueue;
    dispatchQueue.swap(m_pendingEvents);

    for (const Event& eventData : dispatchQueue)
    {
        auto it = m_subscriptions.find(eventData.name);
        if (it == m_subscriptions.end())
        {
            continue;
        }

        const auto listeners = it->second;
        for (const Subscription& subscription : listeners)
        {
            if (subscription.handler)
            {
                subscription.handler(eventData);
            }
        }
    }
}

void EventBus::shutdown()
{
    m_pendingEvents.clear();
    m_subscriptions.clear();
    m_nextToken = 1;
}