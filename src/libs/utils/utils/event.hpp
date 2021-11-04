#pragma once

#include <vector>
#include <algorithm>
#include <functional>

namespace sandbox::utils
{
    namespace detail
    {
        template<typename T>
        class event_emitter
        {
        public:
            virtual ~event_emitter() = default;

            void emit_event(const T& value)
            {
                for (auto l : m_listenners) {
                    l(value);
                }
            }

            uint32_t subscribe_listenner(std::function<void(const T&)> l)
            {
                m_listenners.emplace_back(l);
                return m_listenners.size() - 1;
            }

            void unsubscribe_listenner(uint32_t idx)
            {
                uint32_t i{0};
                m_listenners.erase(std::remove_if(m_listenners.begin(), m_listenners.end(), [&i, idx](const auto& l) { return i++ == idx; }), m_listenners.end());
            }

        private:
            std::vector<std::function<void(const T&)>> m_listenners;
        };
    } // namespace detail


    template<typename... Types>
    class event_emitter : public detail::event_emitter<Types>...
    {
    public:
        template<typename T>
        void unsubscribe_listenner(uint32_t l)
        {
            detail::event_emitter<T>::unsubscribe_listenner(l);
        }

        using detail::event_emitter<Types>::subscribe_listenner...;
        using detail::event_emitter<Types>::emit_event...;
    };
} // namespace sandbox::utils
