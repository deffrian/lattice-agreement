#pragma once

#include "../logger.h"

namespace net {

    struct Message {
        uint64_t size;
        std::vector<uint8_t> data;
        uint64_t cur_pos = 0;

        template<typename T>
        Message& operator<<(const T &val) {
            static_assert(std::is_standard_layout<T>::value, "Incorrect type");
            size_t cur_size = data.size();
            data.resize(cur_size + sizeof(T));
            memcpy(data.data() + cur_size, &val, sizeof(T));
            size = data.size();
            return *this;
        }

        template<>
        Message& operator<<(const LatticeSet &val) {
            *this << val.set.size();
            for (const auto &elem : val.set) {
                *this << elem;
            }
            return *this;
        }

        template<typename T>
        Message& operator<<(const std::vector<T> &val) {
            *this << val.size();
            for (const auto &elem : val) {
                *this << elem;
            }
            return *this;
        }

        template<typename L, typename R>
        Message& operator<<(const std::pair<L, R> &val) {
            *this << val.first;
            *this << val.second;
            return *this;
        }

        template<typename T>
        Message& operator>>(T &val) {
            static_assert(std::is_standard_layout<T>::value, "Incorrect type");

            if (data.size() < cur_pos + sizeof(T)) {
                LOG(ERROR) << "Invalid read";
                throw std::runtime_error("Invalid read");
            }

            memcpy(&val, data.data() + cur_pos, sizeof(T));

            cur_pos += sizeof(T);
            return *this;
        }

        template<>
        Message& operator>>(LatticeSet &val) {
            uint64_t size;
            *this >> size;
            for (uint64_t i = 0; i < size; ++i) {
                uint64_t elem;
                *this >> elem;
                val.insert(elem);
            }
            return *this;
        }

        template<typename T>
        Message &operator>>(std::vector<T> &val) {
            uint64_t size;
            *this >> size;
            for (uint64_t i = 0; i < size; ++i) {
                T elem;
                *this >> elem;
                val.push_back(elem);
            }
            return *this;
        }

        template<typename L, typename R>
        Message& operator>>(std::pair<L, R> &val) {
            *this >> val.first;
            *this >> val.second;
            return *this;
        }
    };

    struct IMessageReceivedCallback {
        virtual void on_message_received(Message &message) = 0;

        virtual ~IMessageReceivedCallback() = default;
    };
}