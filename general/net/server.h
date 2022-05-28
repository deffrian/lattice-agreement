#pragma once

#include <thread>
#include <random>
#include <queue>

#include <asio.hpp>

#include "connection.h"
#include "message.h"

namespace net {

    struct Server : IMessageReceivedCallback {
        asio::io_context context;

        std::vector<std::thread> context_threads;

        std::thread message_thread;
        asio::ip::tcp::acceptor asio_acceptor;

        IMessageReceivedCallback *callback;

        std::unordered_map<uint64_t, std::shared_ptr<WriteConnection>> write_connections;

        std::mutex queue_mt;
        std::condition_variable queue_cv;
        std::queue<Message> message_queue;

        std::atomic<bool> should_stop;

        Server(IMessageReceivedCallback *callback, uint64_t port)
                : callback(callback),
                  asio_acceptor(context,
                                asio::ip::tcp::endpoint(
                                        asio::ip::tcp::v4(),
                                        port)) {
        }

        void start(const std::vector<ProcessDescriptor> &descriptors) {
//            std::this_thread::sleep_for(std::chrono::milliseconds((uint64_t)distribution(generator)));
            accept_connection();
            for (size_t i = 0; i < 5; ++i) {
                context_threads.emplace_back([&]() {
                    context.run();
                });
            }

            for (const auto &descriptor: descriptors) {
                auto connection = std::make_shared<net::WriteConnection>(context, descriptor);
                write_connections[descriptor.id] = connection;
            }

            message_thread = std::thread([&]() {
                std::unique_lock ul(queue_mt);
                while(!should_stop) {
                    queue_cv.wait(ul);
                    while (!message_queue.empty()) {
                        if (message_queue.size() > 10) {
                            LOG(ERROR) << "SIZE" << message_queue.size();
                        }
                        Message message = message_queue.front();
                        message_queue.pop();
                        ul.unlock();
                        callback->on_message_received(message);
                        ul.lock();
                    }
                }
            });
        }

        void stop() {
            for (auto &context_thread: context_threads) {
                if (context_thread.joinable()) context_thread.join();
            }
            should_stop = true;
            queue_cv.notify_one();
            if (message_thread.joinable()) message_thread.join();
        }

        void send(const ProcessDescriptor &descriptor, const Message &message) {
            if (write_connections.count(descriptor.id) == 0) {
                auto connection = std::make_shared<net::WriteConnection>(context, descriptor);
                write_connections[descriptor.id] = connection;
            }
            write_connections[descriptor.id]->send(message);
        }

        void on_message_received(Message &message) override {
            std::lock_guard lg(queue_mt);
            message_queue.push(message);
            queue_cv.notify_one();
        }

    private:

        std::vector<std::shared_ptr<ReadConnection>> read_connections;

        void accept_connection() {
            asio_acceptor.async_accept([&](std::error_code e, asio::ip::tcp::socket socket) {
                accept_connection();
                if (!e) {
                    auto connection = std::make_shared<ReadConnection>(context, std::move(socket), this);
                    connection->receive();
                    read_connections.push_back(connection);
                } else {
                    LOG(ERROR) << "Error accepting" << e.message();
                }
            });
        }
    };

}