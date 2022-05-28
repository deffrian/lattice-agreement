#pragma once

#include "message.h"
#include "net_async.h"

namespace net {

    struct ReadConnection : std::enable_shared_from_this<ReadConnection> {

        asio::io_context &context;
        asio::ip::tcp::socket socket;

        IMessageReceivedCallback *callback;

        Message message;

        ReadConnection(asio::io_context &context, asio::ip::tcp::socket socket, IMessageReceivedCallback *callback)
                : context(context),
                  socket(std::move(socket)),
                  callback(callback) {}

        void receive() {
            read_header();
        }

    private:
        void read_header() {
            asio::async_read(socket, asio::buffer(&message.size, sizeof(message.size)),
                [&](std::error_code er, size_t len) {
                    if (!er) {
                        message.data.resize(message.size);
                        read_data();
                    } else {
                        LOG(ERROR) << "Error reading header:" << er.message();
                        socket.close();
                    }
                });
        }

        void read_data() {
            asio::async_read(socket, asio::buffer(message.data.data(), message.size),
                [&](std::error_code er, size_t len) {
                    if (!er) {
                        callback->on_message_received(message);
                        read_header();
                    } else {
                        LOG(ERROR) << "Error reading data:" << er.message();
                    }
                });
        }
    };

    struct WriteConnection : std::enable_shared_from_this<WriteConnection> {

        asio::io_context &context;
        asio::ip::tcp::socket socket;

        ProcessDescriptor descriptor;

        std::mutex mt;
        std::queue<Message> messages;
        Message cur_message;

        uint64_t attempt = 0;

        WriteConnection(asio::io_context &context, ProcessDescriptor descriptor)
                : context(context),
                  socket(context),
                  descriptor(std::move(descriptor)) {
            connect();
        }

        void send(Message message) {
            asio::post(context, [&, message] {
                std::lock_guard lg{mt};
//                if (messages.size() > 10) {
//                    LOG(ERROR) << "Queue" << messages.size();
//                }
                messages.push(message);
                if (messages.size() == 1) {
                    cur_message = message;
                    write_header();
                }
            });
        }

    private:
        void next_message() {
            std::lock_guard lg{mt};
            messages.pop();
            if (!messages.empty()) {
                Message message = messages.front();
                cur_message = message;
                write_header();
            }
        }

        void connect() {
            asio::ip::tcp::resolver resolver(context);
            asio::ip::tcp::resolver::results_type endpoints
                    = resolver.resolve(descriptor.ip_address, std::to_string(descriptor.port));
//            self->socket.
            asio::async_connect(socket, endpoints, [&](std::error_code er, const asio::ip::tcp::endpoint &endpoint) {
                if (!er) {
                } else {
                    socket.close();
                    attempt++;
                    if (attempt < 10) {
                        LOG(ERROR) << "Unable to connect:" << er.message() << attempt;
                        connect();
                    } else {
                        LOG(ERROR) << "Unable to connect:" << er.message();
                    }
                }
            });
        }

        void write_header() {
            asio::async_write(socket, asio::buffer(&cur_message.size, sizeof(cur_message.size)),
                [&](std::error_code er, size_t len) {
                    if (!er) {
                        write_data();
                    } else {
                        LOG(ERROR) << "Error writing header:" << er.message();
                        socket.close();
                    }
                });
        }

        void write_data() {
            asio::async_write(socket, asio::buffer(cur_message.data.data(), cur_message.size),
                [&](std::error_code er, size_t len) {
                    if (!er) {
                        next_message();
                    } else {
                        LOG(ERROR) << "Error writing data:" << er.message();
                        socket.close();
                    }
                });
        }
    };
}