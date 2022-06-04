#pragma once

#include "message.h"
#include "net_async.h"

namespace net {

    /**
     * Reads message from socket.
     */
    struct ReadConnection : std::enable_shared_from_this<ReadConnection> {

        /**
         * ReadConnection constructor
         * @param context context where async calls will be executed
         * @param socket client socket
         * @param callback message received callback
         */
        ReadConnection(asio::io_context &context, asio::ip::tcp::socket socket, IMessageReceivedCallback *callback)
                : context(context),
                  socket(std::move(socket)),
                  callback(callback) {}

        /**
         * Start async task
         */
        void receive() {
            read_header(shared_from_this());
        }

    private:
        asio::io_context &context;
        asio::ip::tcp::socket socket;

        IMessageReceivedCallback *callback;

        Message message;

        static void read_header(std::shared_ptr<ReadConnection> self) {
            asio::async_read(self->socket, asio::buffer(&self->message.size, sizeof(self->message.size)),
                [&, self](std::error_code er, size_t len) {
                    if (!er) {
                        self->message.data.resize(self->message.size);
                        read_data(self);
                    } else {
                        LOG(ERROR) << "Error reading header:" << er.message();
                        self->socket.close();
                    }
                });
        }

        static void read_data(std::shared_ptr<ReadConnection> self) {
            asio::async_read(self->socket, asio::buffer(self->message.data.data(), self->message.size),
                [&, self](std::error_code er, size_t len) {
                    self->socket.close();
                    if (!er) {
                        self->callback->on_message_received(self->message);
                    } else {
                        LOG(ERROR) << "Error reading data:" << er.message();
                    }
                });
        }
    };

    struct WriteConnection : std::enable_shared_from_this<WriteConnection> {

        asio::io_context &context;
        asio::ip::tcp::socket socket;
        Message message;
        ProcessDescriptor descriptor;
        asio::steady_timer timer;

        WriteConnection(asio::io_context &context, ProcessDescriptor descriptor, Message message)
                : message(std::move(message)),
                  context(context),
                  socket(context),
                  timer(context),
                  descriptor(std::move(descriptor)) {}

        void send() {
            auto ptr = shared_from_this();
            timer.async_wait([ptr](const asio::error_code& er) {
                if (!er) {
                    connect(ptr);
                } else {
                    LOG(ERROR) << "ERROR Waiting" << er.message();
                }
            });
        }

    private:
        static void connect(std::shared_ptr<WriteConnection> self) {
            asio::ip::tcp::resolver resolver(self->context);
            asio::ip::tcp::resolver::results_type endpoints
                    = resolver.resolve(self->descriptor.ip_address, std::to_string(self->descriptor.port));
            asio::async_connect(self->socket, endpoints, [&, self](std::error_code er, const asio::ip::tcp::endpoint &endpoint) {
                if (!er) {
                    write_header(self);
                } else {
                    LOG(ERROR) << "Unable to connect:" << er.message();
                    self->socket.close();
                }
            });
        }

        static void write_header(std::shared_ptr<WriteConnection> self) {
            asio::async_write(self->socket, asio::buffer(&self->message.size, sizeof(self->message.size)),
                [&, self](std::error_code er, size_t len) {
                    if (!er) {
                        write_data(self);
                    } else {
                        LOG(ERROR) << "Error writing header:" << er.message();
                        self->socket.close();
                    }
                });
        }

        static void write_data(std::shared_ptr<WriteConnection> self) {
            asio::async_write(self->socket, asio::buffer(self->message.data.data(), self->message.size),
                [&, self](std::error_code er, size_t len) {
                    if (er) {
                        LOG(ERROR) << "Error writing data:" << er.message();
                    }
                    self->socket.close();
                });
        }
    };
}