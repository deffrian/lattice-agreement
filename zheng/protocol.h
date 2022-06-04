#pragma once

#include <thread>
#include <map>
#include <atomic>
#include <unordered_map>

#include "general/net/server.h"
#include "general/logger.h"

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
struct ProtocolTcp : net::IMessageReceivedCallback {

    std::atomic<uint64_t> message_id;

    std::atomic<uint64_t> message_cnt = 0;

    std::unordered_map<uint64_t, net::ProcessDescriptor> processes;

    net::Server server;

    explicit ProtocolTcp(uint64_t port, uint64_t id) : server(this, port), message_id(id * 1000) {}

    void add_process(const net::ProcessDescriptor &descriptor) {
        processes[descriptor.id] = descriptor;
    }

    Callback<L> *callback;

    void start(Callback<L> *callback) {
        this->callback = callback;
        LOG(INFO) << "starting server thread processes cnt: " << processes.size();
        server.start();
    }

    void stop() {
        server.stop();
    }

private:

    void on_message_received(net::Message &message) override {
        uint64_t from = UINT64_MAX;
        uint64_t message_id_rec = UINT64_MAX;
        try {
//            auto client_fd = value.first;
            uint8_t message_type;
            message >> message_type >> from >> message_id_rec;
            LOG(INFO) << "New connection from" << from << "message_id:" << message_id_rec << "type:"
                      << (int) message_type;
            if (message_type == Value) {
                std::vector<L> lv;
                message >> lv;
                callback->receive_value(lv, message_id_rec);
            } else if (message_type == Write) {
                std::vector<L> val;
                double k;
                uint64_t r;
                message >> val >> k >> r;
                callback->receive_write(val, k, r, from, message_id_rec);
            } else if (message_type == Read) {
                uint64_t r;
                message >> r;
                callback->receive_read(r, from, message_id_rec);
            } else if (message_type == WriteAck) {
                std::vector<std::pair<std::vector<L>, double>> val;
                uint64_t r;
                message >> val >> r;
                callback->receive_write_ack(val, r, message_id_rec);
            } else if (message_type == ReadAck) {
                std::vector<std::pair<std::vector<L>, double>> recVal;
                uint64_t r;
                message >> recVal >> r;
                callback->receive_read_ack(recVal, r, message_id_rec);
            } else {
                LOG(ERROR) << "Unknown message type" << (int)message_type;
                throw std::runtime_error("Unknown message type " + std::to_string((int)message_type));
            }
        } catch (std::runtime_error &e) {
            LOG(ERROR) << "* Exception while processing client. message id:" << message_id_rec << "from:" << from
                       << e.what();
        }
    }

public:
    void send_write(const std::vector<L> &v, double k, uint64_t r, uint64_t from) {
        message_cnt++;
        std::thread([&, v, k, r, from]() {
            uint8_t message_type = Write;
            net::Message message;
            message << message_type << from << message_id++ << v << k << r;
            for (const auto &descriptor: processes) {
                try {
                    LOG(INFO) << ">> sending write to" << descriptor.second.id << "from" << from << "message id"
                              << message_id;
//                    auto client = get_socket(descriptor.second);


//                    send_byte(client, message_type);
//                    send_number(client, from);
//                    send_number(client, cur_message_id);
//                    send_lattice_vector(client, v);
//                    send_double(client, k);
//                    send_number(client, r);
//                    free_socket(client);
                    server.send(descriptor.second, message);
                } catch (std::runtime_error &e) {
                    LOG(ERROR) << "* Exception while send_write" << e.what();
                }
            }
        }).detach();
    }

    void send_read(uint64_t r, uint64_t from) {
        message_cnt++;
        std::thread([&, r, from]() {
            uint8_t message_type = Read;
            net::Message message;
            message << message_type << from << message_id++ << r;
            for (const auto &descriptor: processes) {
                try {
                    uint64_t cur_message_id = message_id++;
                    LOG(INFO) << ">> sending read to" << descriptor.second.id << "cur message id:" << cur_message_id;
//                    auto client = get_socket(descriptor.second);
//
//                    send_byte(client, message_type);
//                    send_number(client, from);
//                    send_number(client, cur_message_id);
//                    send_number(client, r);
//                    free_socket(client);
                    server.send(descriptor.second, message);
                } catch (std::runtime_error &e) {
                    LOG(ERROR) << "* Exception while send_read" << e.what();
                }
            }
        }).detach();
    }

    void send_write_ack(uint64_t to, const std::vector<std::pair<std::vector<L>, double>> &recVal, uint64_t rec_r, uint64_t from, uint64_t cur_message_id) {
        message_cnt++;
        uint8_t message_type = WriteAck;
        try {
            LOG(INFO) << ">> sending write ack to " << to << "cur message id:" << cur_message_id;

//            auto client = get_socket(processes.at(to));

            net::Message message;
            message << message_type << from << cur_message_id << recVal << rec_r;
            server.send(processes.at(to), message);
//            send_byte(client, message_type);
//            send_number(client, from);
//            send_number(client, cur_message_id);
//            send_recVal(client, recVal);
//            send_number(client, rec_r);
//            free_socket(client);
        } catch (std::runtime_error &e) {
            LOG(ERROR) << "* Exception while send_write_ack" << e.what();
        }
    }

    void send_read_ack(uint64_t to, const std::vector<std::pair<std::vector<L>, double>> &recVal, uint64_t r, uint64_t from, uint64_t cur_message_id) {
        message_cnt++;
        uint8_t message_type = ReadAck;
        try {
            LOG(INFO) << ">> sending read ack to" << to << "cur message id:" << cur_message_id;
            net::Message message;

            message << message_type << from << cur_message_id << recVal << r;
            server.send(processes.at(to), message);

//            auto client = get_socket(processes.at(to));
//            send_byte(client, message_type);
//            send_number(client, from);
//            send_number(client, cur_message_id);
//            send_recVal(client, recVal);
//            send_number(client, r);
//            free_socket(client);
        } catch (std::runtime_error &e) {
            LOG(ERROR) << "* Exception while send_read_ack" << e.what();
        }
    }

    void send_value(const std::vector<L> &v, uint64_t from) {
        message_cnt++;
        std::thread([&, v, from]() {
            uint8_t message_type = Value;
            net::Message message;
            message << message_type << from << message_id++ << v;
            for (const auto &descriptor: processes) {
                try {
                    LOG(INFO) << ">> sending value to " << descriptor.second.id;
//                    auto client = get_socket(descriptor.second);
//
//                    send_byte(client, message_type);
//                    send_number(client, from);
//                    send_number(client, cur_message_id);
//                    send_lattice_vector(client, v);
//                    free_socket(client);
                    server.send(descriptor.second, message);
                } catch (std::runtime_error &e) {
                    LOG(ERROR) << "* Exception while send_value" << e.what();
                }
            }

        }).detach();
    }
};
