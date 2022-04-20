#pragma once

#include <string>
#include <thread>
#include <map>

#include "network.h"

enum MessageType : uint8_t {
    Write = 0,
    Read = 1,
    WriteAck = 2,
    ReadAck = 3,
    Value = 4
};

template<typename L>
struct Callback {
    virtual void receive_write_ack(const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal, uint64_t rec_r) = 0;

    virtual void receive_read_ack(const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal, uint64_t rec_r) = 0;

    virtual void receive_value(const std::vector<L> &value) = 0;

    virtual void receive_write(const std::vector<L> &value, uint64_t k, uint64_t rec_r, uint64_t from) = 0;

    virtual void receive_read(uint64_t rec_r, uint64_t from) = 0;

    virtual ~Callback() = default;
};

template<typename L>
struct ProtocolTcp {

    std::atomic_bool should_stop = false;

    std::map<uint64_t, ProcessDescriptor> processes;

    TcpServer server;

    explicit ProtocolTcp(uint64_t port) : server(port) {}

    void add_process(const ProcessDescriptor &descriptor) {
        processes[descriptor.id] = descriptor;
    }

    void start(Callback<L> *callback) {
        std::cout << "starting server thread processes cnt: " << processes.size() << std::endl;
        std::thread(
                [&](Callback<L> *callback) {
                    while (!should_stop) {
                        int new_socket = server.accept_client();
                        if (new_socket < 0) {
                            std::cout << "Server thread failed" << std::endl;
                            return;
                        }
                        process_client(new_socket, callback);
                    }
                },
                callback).detach();
    }

    void stop() {
        should_stop = true;
    }

private:

    static std::vector<std::pair<std::vector<L>, uint64_t>> read_recVal(int client_fd) {
        std::vector<std::pair<std::vector<L>, uint64_t>> recVal;
        uint64_t recVal_size = read_number(client_fd);
        for (uint64_t i = 0; i < recVal_size; ++i) {
            std::vector<L> l = read_lattice_vector<L>(client_fd);
            uint64_t n = read_number(client_fd);
            recVal.emplace_back(l, n);
        }
        return recVal;
    }

    static void process_client(int client_fd, Callback<L> *callback) {
        std::thread([=] {
            uint8_t message_type;
            ssize_t len = read(client_fd, &message_type, 1);
            if (len != 1) {
                assert(false);
            }
            uint64_t from = read_number(client_fd);
//            std::cout << "New connection from " << from << std::endl;
            if (message_type == Value) {
                callback->receive_value(read_lattice_vector<L>(client_fd));
            } else if (message_type == Write) {
                auto val = read_lattice_vector<L>(client_fd);
                uint64_t k = read_number(client_fd);
                uint64_t r = read_number(client_fd);
                callback->receive_write(val, k, r, from);
            } else if (message_type == Read) {
                uint64_t r = read_number(client_fd);
                callback->receive_read(r, from);
            } else if (message_type == WriteAck) {
                auto val = read_recVal(client_fd);
                uint64_t r = read_number(client_fd);
                callback->receive_write_ack(val, r);
            } else if (message_type == ReadAck) {
                auto recVal = read_recVal(client_fd);
                uint64_t r = read_number(client_fd);
                callback->receive_read_ack(recVal, r);
            } else {
                std::cout << "unknown message" << std::endl;
                assert(false);
            }
            close(client_fd);
        }).detach();
    }

    void send_recVal(int sock, const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal) {
        send_number(sock, recVal.size());
        for (auto elem: recVal) {
            send_lattice_vector(sock, elem.first);
            send_number(sock, elem.second);
        }
    }

public:
    void send_write(const std::vector<L> &v, uint64_t k, uint64_t r, uint64_t from) {
        uint8_t message_type = Write;
        for (const auto &descriptor : processes) {
            int sock = open_socket(descriptor.second);
            std::cout << ">> sending write to " << descriptor.second.id << std::endl;

            send(sock, &message_type, 1, 0);
            send_number(sock,from);
            send_number(sock, v.size());
            for (size_t i = 0; i < v.size(); ++i) {
                send_lattice(sock, v[i]);
            }
            send_number(sock, k);
            send_number(sock, r);
            close(sock);
//            std::cout << "Connection to " << descriptor.second.id << " closed" << std::endl;
        }
    }

    void send_read(uint64_t r, uint64_t from) {
        uint8_t message_type = Read;
        for (const auto &descriptor : processes) {
            int sock = open_socket(descriptor.second);
            std::cout << ">> sending read to " << descriptor.second.id << std::endl;

            send(sock, &message_type, 1, 0);
            send_number(sock, from);
            send_number(sock, r);
            close(sock);
//            std::cout << "Connection to " << descriptor.second.id << "closed" << std::endl;
        }
    }

    void send_write_ack(uint64_t to, const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal, uint64_t rec_r, uint64_t from) {
        uint8_t message_type = WriteAck;

        int sock = open_socket(processes[to]);
        std::cout << ">> sending write ack to " << to << std::endl;

        send(sock, &message_type, 1, 0);
        send_number(sock, from);
        send_recVal(sock, recVal);
        send_number(sock, rec_r);
        close(sock);
//        std::cout << "Connection to " << to << "closed" << std::endl;
    }

    void send_read_ack(uint64_t to, const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal, uint64_t r, uint64_t from) {
        uint8_t message_type = ReadAck;

        int sock = open_socket(processes[to]);
        std::cout << ">> sending read ack to " << to << std::endl;

        send(sock, &message_type, 1, 0);
        send_number(sock,from);
        send_recVal(sock, recVal);
        send_number(sock,r);
        close(sock);
//        std::cout << "Connection to " << to << "closed" << std::endl;
    }

    void send_value(const std::vector<L> &v, uint64_t from) {
        uint8_t message_type = Value;
        for (const auto &descriptor : processes) {
            int sock = open_socket(descriptor.second);
            std::cout << ">> sending value to " << descriptor.second.id << std::endl;

            send(sock, &message_type, 1, 0);
            send_number(sock, from);
            send_number(sock, v.size());
            for (size_t i = 0; i < v.size(); ++i) {
                send_lattice(sock, v[i]);
            }
            close(sock);
//            std::cout << "Connection to " << descriptor.second.id << "closed" << std::endl;
        }
    }
};
