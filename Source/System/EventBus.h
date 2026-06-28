#pragma once

#include <any>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

//=====================================================
// Lightweight event bus that flushes at the frame boundary.
//=====================================================
class EventBus
{
public:

    struct Event
    {
        std::string name;
        uint64_t senderId = 0;
        std::any payload;

        template<class T>
        const T* payloadAs() const
        {
            return std::any_cast<T>(&payload);
        }
    };

    using Handler = std::function<void(const Event&)>;
    using SubscriptionToken = uint64_t;

    static EventBus& Instance()
    {
        static EventBus instance;
        return instance;
    }

    SubscriptionToken subscribe(const std::string& eventName, Handler handler);
    void unsubscribe(SubscriptionToken token);

    void publish(std::string eventName, uint64_t senderId = 0);
    void publish(std::string eventName, std::any payload, uint64_t senderId = 0);

    void dispatchQueued();
    void shutdown();

private:

    struct Subscription
    {
        SubscriptionToken token = 0;
        Handler handler;
    };

    EventBus() = default;
    ~EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    SubscriptionToken m_nextToken = 1;
    std::unordered_map<std::string, std::vector<Subscription>> m_subscriptions;
    std::vector<Event> m_pendingEvents;
};