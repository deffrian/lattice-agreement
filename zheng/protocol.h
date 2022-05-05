#pragma once

#include <string>
#include <thread>
#include <map>
#include <atomic>
#include <queue>

#include "general/network.h"
#include "general/thread_pool.h"
#include "general/tcp_client.h"

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

    std::unordered_map<uint64_t, TcpClient> known_sockets;

    std::atomic<bool> should_stop = false;
    std::atomic<uint64_t> message_id;

    std::unordered_map<uint64_t, ProcessDescriptor> processes;

    ThreadPool<std::pair<int, Callback<L> *>> clients_processors;

    TcpServer server;

    explicit ProtocolTcp(uint64_t port, uint64_t id) : server(port), message_id(id * 1000),
                                                       clients_processors([&](std::pair<int, Callback<L> *> &value) {
                                                           this->process_client(value);
                                                       }, 3) {}

    void add_process(const ProcessDescriptor &descriptor) {
        processes[descriptor.id] = descriptor;
    }

    void start(Callback<L> *callback) {
        LOG(INFO) << "starting server thread processes cnt: " << processes.size();
        std::thread(
                [&, callback]() {
                    LOG(INFO) << "Waiting for connections";
                    for (const auto &peer: processes) {
                        int new_socket = server.accept_client();
                        if (new_socket < 0) {
                            LOG(ERROR) << "Error while accepting client";
                            throw std::runtime_error("Select client failed");
                        }
                    }
                    while (!should_stop) {
                        std::vector<int> active_sockets = server.select_clients();

                        for (int socket: active_sockets) {
                            clients_processors.add_job({socket, callback});
                        }
                    }
                }).detach();
    }

    void open_sockets() {
        LOG(INFO) << "Establish connections to peers";
        for (const auto &peer : processes) {
            known_sockets.emplace(peer.first, peer.second);
        }
        LOG(INFO) << "All peers connected";
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


    void process_client(std::pair<int, Callback<L> *> &value) {
        auto callback = value.second;
        auto client_fd = value.first;
        uint8_t message_type;
        ssize_t len = read(client_fd, &message_type, 1);
        if (len == 0) {
            LOG(ERROR) << "Connection closed";
            close(client_fd);
            return;
        }
        if (len != 1) {
            LOG(ERROR) << "Can't read message type" << errno;
            exit(EXIT_FAILURE);
        }
        uint64_t from = read_number(client_fd);
        uint64_t message_id_rec = read_number(client_fd);
        LOG(INFO) << "New connection from" << from << "message_id:" << message_id_rec << "type:"
                  << (int) message_type;
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
            LOG(ERROR) << "unknown message";
            exit(EXIT_FAILURE);
        }
        server.close_socket(client_fd);
    }

    std::mutex sock_mt;

    TcpClient &get_socket(const ProcessDescriptor &descriptor) {
        sock_mt.lock();
        return known_sockets.at(descriptor.id);
    }

    void free_socket(const ProcessDescriptor &descriptor) {
        sock_mt.unlock();
    }

public:
    void send_write(const std::vector<L> &v, uint64_t k, uint64_t r, uint64_t from) {
        std::thread([&, v, k, r, from]() {
            uint8_t message_type = Write;
            for (const auto &descriptor: processes) {
                uint64_t cur_message_id = message_id++;
                LOG(INFO) << ">> sending write to" << descriptor.second.id << "from" << from << "message id"
                          << cur_message_id;
                auto client = get_socket(descriptor.second);

                client.send_byte(message_type);
                client.send_number(from);
                client.send_number(cur_message_id);
                client.send_lattice_vector(v);
                client.send_number(k);
                client.send_number(r);
                free_socket(descriptor.second);
            }
        }).detach();
    }

    void send_read(uint64_t r, uint64_t from) {
        std::thread([&, r, from]() {
            uint8_t message_type = Read;
            for (const auto &descriptor: processes) {
                uint64_t cur_message_id = message_id++;
                LOG(INFO) << ">> sending read to" << descriptor.second.id << "cur message id:" <<cur_message_id;
                auto client = get_socket(descriptor.second);

                client.send_byte(message_type);
                client.send_number(from);
                client.send_number(cur_message_id);
                client.send_number(r);
                free_socket(descriptor.second);
            }
        }).detach();
    }

    void send_write_ack(uint64_t to, const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal, uint64_t rec_r, uint64_t from, uint64_t cur_message_id) {
        uint8_t message_type = WriteAck;

        LOG(INFO) << ">> sending write ack to " << to << "cur message id:" << cur_message_id;
        auto client = get_socket(processes.at(to));

        client.send_byte(message_type);
        client.send_number(from);
        client.send_number(cur_message_id);
        client.send_recVal(recVal);
        client.send_number(rec_r);
        free_socket(processes.at(to));
    }

    void send_read_ack(uint64_t to, const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal, uint64_t r, uint64_t from, uint64_t cur_message_id) {
        uint8_t message_type = ReadAck;

        LOG(INFO) << ">> sending read ack to" << to << "cur message id:" << cur_message_id;
        auto client = get_socket(processes.at(to));

        client.send_byte(message_type);
        client.send_number(from);
        client.send_number(cur_message_id);
        client.send_recVal(recVal);
        client.send_number(r);
        free_socket(processes.at(to));
    }

    void send_value(const std::vector<L> &v, uint64_t from) {
        std::thread([&, v, from]() {
            uint8_t message_type = Value;
            for (const auto &descriptor: processes) {
                uint64_t cur_message_id = message_id++;
                LOG(INFO) << ">> sending value to " << descriptor.second.id << cur_message_id;
                auto client = get_socket(descriptor.second);

                client.send_byte(message_type);
                client.send_number(from);
                client.send_number(cur_message_id);
                client.send_lattice_vector(v);
                free_socket(descriptor.second);
            }
        }).detach();
    }
};
