#pragma once

#include <thread>
#include <random>

#include <asio.hpp>

#include "connection.h"
#include "message.h"

namespace net {

    struct Server : IMessageReceivedCallback {
        asio::io_context context;
        std::thread context_thread;
        asio::ip::tcp::acceptor asio_acceptor;

        IMessageReceivedCallback *callback;

        std::default_random_engine generator;
        std::normal_distribution<double> distribution{100, 10};

        Server(IMessageReceivedCallback *callback, uint64_t port)
                : callback(callback),
                  asio_acceptor(context,
                                asio::ip::tcp::endpoint(
                                        asio::ip::tcp::v4(),
                                        port)) {}

        void start() {
//            std::this_thread::sleep_for(std::chrono::milliseconds((uint64_t)distribution(generator)));
            accept_connection();
            context_thread = std::thread([&]() {
                context.run();
            });
        }

        void stop() {
            context.stop();
            if (context_thread.joinable()) context_thread.join();
        }

        void send(const ProcessDescriptor &descriptor, const Message &message) {
            auto connection = std::make_shared<net::WriteConnection>(context,  descriptor, message);
            connection->send();
        }

        void on_message_received(Message &message) override {
            callback->on_message_received(message);
        }

    private:
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