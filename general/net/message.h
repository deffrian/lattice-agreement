#pragma once

#include "../logger.h"

namespace net {

    /**
     * Message type.
     * Used by LA Protocols to serialize and deserialize messages using "<<" and ">>" operators and communicate with each other.
     */
    struct Message {

        /**
         * Serializes trivial type and writes it to buffer.
         * @tparam T Serializable type. Should be trivially copiable
         * @param val Serializable value
         * @return reference to this
         */
        template<typename T>
        Message& operator<<(const T &val) {
            size_t cur_size = data.size();
            data.resize(cur_size + sizeof(T));
            memcpy(data.data() + cur_size, &val, sizeof(T));
            size = data.size();
            return *this;
        }

        /**
         * Serializes @LatticeSet and writes it to buffer.
         * @param val Serializable set
         * @return reference to this
         */
        template<>
        Message& operator<<(const LatticeSet &val) {
            *this << val.set.size();
            for (const auto &elem : val.set) {
                *this << elem;
            }
            return *this;
        }

        /**
         * Serializes @std::vector and writes it to buffer.
         * @tparam T Type of vector's contents. Should be serializable using "<<" operator
         * @param val Serializable vector
         * @return reference to this
         */
        template<typename T>
        Message& operator<<(const std::vector<T> &val) {
            *this << val.size();
            for (const auto &elem : val) {
                *this << elem;
            }
            return *this;
        }

        /**
         * Serializes @std::pair and writes it to buffer.
         * @tparam L pair left type
         * @tparam R pair right type
         * @param val Serializable value
         * @return reference to this
         */
        template<typename L, typename R>
        Message& operator<<(const std::pair<L, R> &val) {
            *this << val.first;
            *this << val.second;
            return *this;
        }

        /**
         * Deserializes value.
         * @tparam T Deserializable type. Should be trivially copyable
         * @param val Where deserialized value will be written
         * @return reference to this
         */
        template<typename T>
        Message& operator>>(T &val) {
            if (data.size() < cur_pos + sizeof(T)) {
                LOG(ERROR) << "Invalid read";
                throw std::runtime_error("Invalid read");
            }

            memcpy(&val, data.data() + cur_pos, sizeof(T));

            cur_pos += sizeof(T);
            return *this;
        }

        /**
         * Deserializes @LatticeSet.
         * @param val Where deserialized value will be written
         * @return reference to this
         */
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

        /**
         * Deserializes @std::vector.
         * @tparam T Type of vector's contents. Should be deserializable using ">>" operator
         * @param val Where deserialized value will be written
         * @return reference to this
         */
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

        /**
         * Deserializes @std::pair
         * @tparam L Pair left type
         * @tparam R Pair right type
         * @param val Where deserialized value will be written
         * @return reference to this
         */
        template<typename L, typename R>
        Message& operator>>(std::pair<L, R> &val) {
            *this >> val.first;
            *this >> val.second;
            return *this;
        }

        /**
         * Get size of message in bytes
         * @return message size in bytes
         */
        [[nodiscard]] uint64_t get_size() const {
            return size;
        }

        /**
         * Get pointer to message data
         * @return pointer to message data
         */
        uint8_t *get_data() {
            return data.data();
        }
        
        // size of message in bytes
        uint64_t size;

        // actual message data
        std::vector<uint8_t> data;

        // current position of data pointer
        uint64_t cur_pos = 0;

    };

    /**
     * Message Callback. Used by @Server to notify LA Protocols when message received.
     */

    struct IMessageReceivedCallback {
        /**
         * Called by @Server when message received
         * @param message Message that been received
         */
        virtual void on_message_received(Message &message) = 0;

        virtual ~IMessageReceivedCallback() = default;
    };
}