#pragma once

#include <thread>
#include <random>

#include <asio.hpp>

#include "connection.h"
#include "message.h"

namespace net {

    /**
     * TCP Server. Used by protocols to communicate with each other via sending @Message.
     * Leverages asio library for async TCP communication.
     */
    struct Server : IMessageReceivedCallback {

        /**
         * Server constructor
         * @param callback Protocol callback. Will be called when message received.
         * @param port Listen port
         */
        Server(IMessageReceivedCallback *callback, uint64_t port)
                : callback(callback),
                  asio_acceptor(context,
                                asio::ip::tcp::endpoint(
                                        asio::ip::tcp::v4(),
                                        port)) {}

        /**
         * Start server
         */
        void start() {
            accept_connection();
            context_thread = std::thread([&]() {
                context.run();
            });
        }

        /**
         * Stop server
         */
        void stop() {
            context.stop();
            if (context_thread.joinable()) context_thread.join();
        }

        /**
         * Send message to process with delay
         * @param descriptor Descriptor of receiver
         * @param message Message that will be sent
         */
        void send(const ProcessDescriptor &descriptor, const Message &message) {
            auto connection = std::make_shared<net::WriteConnection>(context,  descriptor, message);
            uint64_t delay = (uint64_t)distribution(generator);
            connection->timer.expires_from_now(std::chrono::milliseconds(delay));
            connection->send();
        }

        void on_message_received(Message &message) override {
            callback->on_message_received(message);
        }

    private:
        // asio context
        asio::io_context context;
        // thread on witch asio context operates
        std::thread context_thread;
        // asio acceptor
        asio::ip::tcp::acceptor asio_acceptor;

        // protocol callback
        IMessageReceivedCallback *callback;

        // message delay
        std::random_device dev;
        std::default_random_engine generator{dev()};
        std::normal_distribution<double> distribution{300, 30};


        void accept_connection() {
            asio_acceptor.async_accept([&](std::error_code e, asio::ip::tcp::socket socket) {
                accept_connection();
                if (!e) {
                    auto connection = std::make_shared<ReadConnection>(context, std::move(socket), this);
                    connection->receive();
                } else {
                    throw std::runtime_error(e.message());
                }
            });
        }
    };

}