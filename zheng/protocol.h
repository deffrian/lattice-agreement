#pragma once

#include <string>
#include <thread>
#include <map>
#include <atomic>
#include <queue>
#include <random>

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
    virtual void receive_write_ack(const std::vector<std::pair<std::vector<L>, double>> &recVal, uint64_t rec_r, uint64_t message_id) = 0;

    virtual void receive_read_ack(const std::vector<std::pair<std::vector<L>, double>> &recVal, uint64_t rec_r, uint64_t message_id) = 0;

    virtual void receive_value(const std::vector<L> &value, uint64_t message_id) = 0;

    virtual void receive_write(const std::vector<L> &value, double k, uint64_t rec_r, uint64_t from, uint64_t message_id) = 0;

    virtual void receive_read(uint64_t rec_r, uint64_t from, uint64_t message_id) = 0;

    virtual ~Callback() = default;
};

template<typename L>
struct ProtocolTcp {

//    std::unordered_map<uint64_t, int> known_sockets;

    std::atomic<bool> should_stop = false;
    std::atomic<uint64_t> message_id;

    std::atomic<uint64_t> message_cnt = 0;

    std::unordered_map<uint64_t, ProcessDescriptor> processes;

    ThreadPool<std::pair<int, Callback<L> *>> clients_processors;

    TcpServer server;

    std::random_device dev;
    std::mt19937 mt{dev()};
    std::uniform_int_distribution<uint64_t> dist{500, 1500}; // 0.5 sec - 1.5 sec

    std::mutex connections_mt;
    std::condition_variable connection_cv;
    const size_t MAX_OUTCOMING_CONNECTIONS = 100000;
    uint64_t connections_number = 0;

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
//                    LOG(INFO) << "Waiting for connections";
//                    for (const auto &peer: processes) {
//                        int new_socket = server.accept_client();
//                        if (new_socket < 0) {
//                            LOG(ERROR) << "Error while accepting client";
//                            throw std::runtime_error("Select client failed");
//                        }
//                    }
                    while (!should_stop) {
//                        std::vector<int> active_sockets = server.select_clients();
                        int socket = server.accept_client();

//                        for (int socket: active_sockets) {
                            clients_processors.add_job({socket, callback});
//                        }
                    }
                }).detach();
    }

    void open_sockets() {
//        LOG(INFO) << "Establish connections to peers";
//        for (const auto &peer : processes) {
//            known_sockets[peer.first] = open_socket(peer.second);
//        }
//        LOG(INFO) << "All peers connected";
    }

    void stop() {
        should_stop = true;
    }

private:

    static std::vector<std::pair<std::vector<L>, double>> read_recVal(int client_fd) {
        std::vector<std::pair<std::vector<L>, double>> recVal;
        uint64_t recVal_size = read_number(client_fd);
        for (uint64_t i = 0; i < recVal_size; ++i) {
            std::vector<L> l = read_lattice_vector<L>(client_fd);
            double n = read_double(client_fd);
            recVal.emplace_back(l, n);
        }
        return recVal;
    }


    void process_client(std::pair<int, Callback<L> *> &value) {
        uint64_t from = UINT64_MAX;
        uint64_t message_id_rec = UINT64_MAX;
        try {
            auto callback = value.second;
            auto client_fd = value.first;
            uint8_t message_type;
            ssize_t len = read(client_fd, &message_type, 1);
            if (len == 0) {
                LOG(INFO) << "Connection closed";
                close(client_fd);
                return;
            }
            if (len != 1) {
                LOG(ERROR) << "Can't read message type" << errno;
                throw std::runtime_error("Can't read message type");
            }
            from = read_number(client_fd);
            message_id_rec = read_number(client_fd);
            LOG(INFO) << "New connection from" << from << "message_id:" << message_id_rec << "type:"
                      << (int) message_type;
            if (message_type == Value) {
                auto lv = read_lattice_vector<L>(client_fd);
                callback->receive_value(lv, message_id_rec);
            } else if (message_type == Write) {
                auto val = read_lattice_vector<L>(client_fd);
                double k = read_double(client_fd);
                LOG(ERROR) << "Receive write" << k;
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
                LOG(ERROR) << "Unknown message type" << (int)message_type;
                throw std::runtime_error("Unknown message type " + std::to_string((int)message_type));
            }
            server.close_socket(client_fd);
        } catch (std::runtime_error &e) {
            LOG(ERROR) << "* Exception while processing client. message id:" << message_id_rec << "from:" << from
                       << e.what();
        }
    }

//    std::mutex sock_mt;

    int get_socket(const ProcessDescriptor &descriptor) {
        {
            std::unique_lock lk{connections_mt};
            while (connections_number >= MAX_OUTCOMING_CONNECTIONS) { connection_cv.wait(lk); }
            connections_number++;
        }
//        std::this_thread::sleep_for(std::chrono::seconds(1));
//        else std::this_thread::sleep_for(std::chrono::seconds(2));
        return open_socket(descriptor);
//        return known_sockets.at(descriptor.id);
    }

    void free_socket(int sock) {
        close(sock);
        std::lock_guard lg{connections_mt};
        connections_number--;
        connection_cv.notify_one();
//        sock_mt.unlock();
    }

public:
    void send_write(const std::vector<L> &v, double k, uint64_t r, uint64_t from) {
        message_cnt++;
        LOG(ERROR) << "send_write" << k;
        std::thread([&, v, k, r, from]() {
            try {
                uint8_t message_type = Write;
                std::this_thread::sleep_for(std::chrono::milliseconds(dist(mt)));
                for (const auto &descriptor: processes) {
                    uint64_t cur_message_id = message_id++;
                    LOG(INFO) << ">> sending write to" << descriptor.second.id << "from" << from << "message id"
                              << cur_message_id;
                    auto client = get_socket(descriptor.second);

                    send_byte(client, message_type);
                    send_number(client, from);
                    send_number(client, cur_message_id);
                    send_lattice_vector(client, v);
                    send_double(client, k);
                    send_number(client, r);
                    free_socket(client);
                }
            } catch (std::runtime_error &e) {
                LOG(ERROR) << "* Exception while send_write" << e.what();
            }
        }).detach();
    }

    void send_read(uint64_t r, uint64_t from) {
        message_cnt++;
        std::thread([&, r, from]() {
            try {
                uint8_t message_type = Read;
                std::this_thread::sleep_for(std::chrono::milliseconds(dist(mt)));
                for (const auto &descriptor: processes) {
                    uint64_t cur_message_id = message_id++;
                    LOG(INFO) << ">> sending read to" << descriptor.second.id << "cur message id:" << cur_message_id;
                    auto client = get_socket(descriptor.second);

                    send_byte(client, message_type);
                    send_number(client, from);
                    send_number(client, cur_message_id);
                    send_number(client, r);
                    free_socket(client);
                }
            } catch (std::runtime_error &e) {
                LOG(ERROR) << "* Exception while send_read" << e.what();
            }
        }).detach();
    }

    void send_write_ack(uint64_t to, const std::vector<std::pair<std::vector<L>, double>> &recVal, uint64_t rec_r, uint64_t from, uint64_t cur_message_id) {
        message_cnt++;
        uint8_t message_type = WriteAck;
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(dist(mt)));
            LOG(INFO) << ">> sending write ack to " << to << "cur message id:" << cur_message_id;
//        LOG(ERROR) << "recVal size" << recVal.size();

            auto client = get_socket(processes.at(to));

            send_byte(client, message_type);
            send_number(client, from);
            send_number(client, cur_message_id);
            send_recVal(client, recVal);
            send_number(client, rec_r);
            free_socket(client);
        } catch (std::runtime_error &e) {
            LOG(ERROR) << "* Exception while send_write_ack" << e.what();
        }
    }

    void send_read_ack(uint64_t to, const std::vector<std::pair<std::vector<L>, double>> &recVal, uint64_t r, uint64_t from, uint64_t cur_message_id) {
        message_cnt++;
        uint8_t message_type = ReadAck;
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(dist(mt)));
            LOG(INFO) << ">> sending read ack to" << to << "cur message id:" << cur_message_id;
            auto client = get_socket(processes.at(to));
            send_byte(client, message_type);
            send_number(client, from);
            send_number(client, cur_message_id);
            send_recVal(client, recVal);
            send_number(client, r);
            free_socket(client);
        } catch (std::runtime_error &e) {
            LOG(ERROR) << "* Exception while send_read_ack" << e.what();
        }
    }

    void send_value(std::vector<L> v, uint64_t from) {
        message_cnt++;
        std::thread([&, v, from]() {
            try {
                uint8_t message_type = Value;
                std::this_thread::sleep_for(std::chrono::milliseconds(dist(mt)));
                for (const auto &descriptor: processes) {
                    uint64_t cur_message_id = message_id++;
                    LOG(INFO) << ">> sending value to " << descriptor.second.id << cur_message_id;
                    auto client = get_socket(descriptor.second);

                    send_byte(client, message_type);
                    send_number(client, from);
                    send_number(client, cur_message_id);
                    send_lattice_vector(client, v);
                    free_socket(client);
                }
            } catch (std::runtime_error &e) {
                LOG(ERROR) << "* Exception while send_value" << e.what();
            }
        }).detach();
    }
};
