#pragma once

#include <string>
#include <thread>
#include <map>
#include <atomic>
#include <queue>

#include "general/network.h"
#include "general/thread_pool.h"

enum MessageType : uint8_t {
    Write = 0,
    Read = 1,
    WriteAck = 2,
    ReadAck = 3,
    Value = 4
};

template<typename L>
struct Callback {
    virtual void receive_write_ack(const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal, uint64_t rec_r, uint64_t message_id) = 0;

    virtual void receive_read_ack(const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal, uint64_t rec_r, uint64_t message_id) = 0;

    virtual void receive_value(const std::vector<L> &value, uint64_t message_id) = 0;

    virtual void receive_write(const std::vector<L> &value, uint64_t k, uint64_t rec_r, uint64_t from, uint64_t message_id) = 0;

    virtual void receive_read(uint64_t rec_r, uint64_t from, uint64_t message_id) = 0;

    virtual ~Callback() = default;
};

template<typename L>
struct ProtocolTcp {

    std::atomic<bool> should_stop = false;
    std::atomic<uint64_t> message_id;

    std::map<uint64_t, ProcessDescriptor> processes;

    ThreadPool<std::pair<int, Callback<L>*>> clients_processors;

    TcpServer server;

    explicit ProtocolTcp(uint64_t port, uint64_t id) : server(port), message_id(id * 100), clients_processors(process_client, 3) {}

    void add_process(const ProcessDescriptor &descriptor) {
        processes[descriptor.id] = descriptor;
    }

    void start(Callback<L> *callback) {
        std::cout << "starting server thread processes cnt: " << processes.size() << std::endl;
        std::thread(
                [&, callback]() {
                    while (!should_stop) {
                        int new_socket = server.select_client();
                        if (new_socket < 0) {
                            std::cout << "Server thread failed" << std::endl;
                            return;
                        }
                        clients_processors.add_job({new_socket, callback});
                    }
                }).detach();
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

    static void process_client(std::pair<int, Callback<L> *> &value) {
        auto callback = value.second;
        auto client_fd = value.first;
        uint8_t message_type;
        ssize_t len = read(client_fd, &message_type, 1);
        if (len == 0) {
            LOG(ERROR) << "Connection closed";
            close(client_fd);
            return;
        }
        if (len < 0) {
            LOG(ERROR) << "Can't read message type" << errno;
            exit(EXIT_FAILURE);
        }
        uint64_t from = read_number(client_fd);
        uint64_t message_id_rec = read_number(client_fd);
        std::cout << "New connection from " << from << " message_id: " << message_id_rec << " type: "
                  << (int) message_type << std::endl;
        if (message_type == Value) {
            auto lv = read_lattice_vector<L>(client_fd);
            callback->receive_value(lv, message_id_rec);
        } else if (message_type == Write) {
            auto val = read_lattice_vector<L>(client_fd);
            uint64_t k = read_number(client_fd);
            uint64_t r = read_number(client_fd);
            callback->receive_write(val, k, r, from, message_id_rec);
        } else if (message_type == Read) {
            uint64_t r = read_number(client_fd);
            callback->receive_read(r, from, message_id_rec);
        } else if (message_type == WriteAck) {
            auto val = read_recVal(client_fd);
            uint64_t r = read_number(client_fd);
            callback->receive_write_ack(val, r, message_id_rec);
        } else if (message_type == ReadAck) {
            auto recVal = read_recVal(client_fd);
            uint64_t r = read_number(client_fd);
            callback->receive_read_ack(recVal, r, message_id_rec);
        } else {
            std::cout << "unknown message" << std::endl;
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }

    void send_recVal(int sock, const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal) {
        send_number(sock, recVal.size());
        for (auto elem: recVal) {
            send_lattice_vector(sock, elem.first);
            send_number(sock, elem.second);
        }
    }

    std::map<uint64_t, int> known_sockets;

    int get_socket(const ProcessDescriptor &descriptor) {
        if (known_sockets.count(descriptor.id) == 0) {
            known_sockets[descriptor.id] = open_socket(descriptor);
        }
        return known_sockets[descriptor.id];
    }

public:
    void send_write(const std::vector<L> &v, uint64_t k, uint64_t r, uint64_t from) {
        std::thread([&, v, k, r, from]() {
            uint8_t message_type = Write;
            for (const auto &descriptor: processes) {
                uint64_t cur_message_id = message_id++;
                std::cout << ">> sending write to " << descriptor.second.id << " from " << from << " message id "
                          << cur_message_id << std::endl;
                int sock = get_socket(descriptor.second);

                send(sock, &message_type, 1, 0);
                send_number(sock, from);
                send_number(sock, cur_message_id);
                send_lattice_vector(sock, v);
                send_number(sock, k);
                send_number(sock, r);
            }
        }).detach();
    }

    void send_read(uint64_t r, uint64_t from) {
        std::thread([&, r, from]() {
            uint8_t message_type = Read;
            for (const auto &descriptor: processes) {
                std::cout << ">> sending read to " << descriptor.second.id << std::endl;
                int sock = get_socket(descriptor.second);

                send(sock, &message_type, 1, 0);
                send_number(sock, from);
                send_number(sock, message_id++);
                send_number(sock, r);
            }
        }).detach();
    }

    void send_write_ack(uint64_t to, const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal, uint64_t rec_r, uint64_t from, uint64_t cur_message_id) {
        uint8_t message_type = WriteAck;

        std::cout << ">> sending write ack to " << to << std::endl;
        int sock = get_socket(processes.at(to));

        send(sock, &message_type, 1, 0);
        send_number(sock, from);
        send_number(sock, cur_message_id);
        send_recVal(sock, recVal);
        send_number(sock, rec_r);
    }

    void send_read_ack(uint64_t to, const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal, uint64_t r, uint64_t from, uint64_t cur_message_id) {
        uint8_t message_type = ReadAck;

        std::cout << ">> sending read ack to " << to << std::endl;
        int sock = get_socket(processes.at(to));

        send(sock, &message_type, 1, 0);
        send_number(sock,from);
        send_number(sock, cur_message_id);
        send_recVal(sock, recVal);
        send_number(sock,r);
    }

    void send_value(const std::vector<L> &v, uint64_t from) {
        std::thread([&, v, from]() {
            uint8_t message_type = Value;
            for (const auto &descriptor: processes) {
                std::cout << ">> sending value to " << descriptor.second.id << std::endl;
                int sock = get_socket(descriptor.second);

                send(sock, &message_type, 1, 0);
                send_number(sock, from);
                send_number(sock, message_id++);
                send_number(sock, v.size());
                for (size_t i = 0; i < v.size(); ++i) {
                    send_lattice(sock, v[i]);
                }
            }
        }).detach();
    }
};
