#pragma once

#include <variant>
#include <vector>
#include "general/network.h"
#include "general/thread_pool.h"

template<typename L>
struct Ack {
    Ack(uint64_t proposal_number, const L &proposed_value, uint64_t proposer_id) : proposal_number(proposal_number),
                                                                             proposed_value(proposed_value),
                                                                             proposer_id(proposer_id) {}

    uint64_t proposal_number;
    L proposed_value;
    uint64_t proposer_id;
};

template<typename L>
struct Nack {
    Nack(uint64_t proposal_number, const L &proposed_value, uint64_t proposer_id) : proposal_number(proposal_number),
                                                                              proposed_value(proposed_value),
                                                                              proposer_id(proposer_id) {}

    uint64_t proposal_number;
    L proposed_value;
    uint64_t proposer_id;
};

template<typename L>
using AcceptorResponse = std::variant<Ack<L>, Nack<L>>;

template<typename L>
struct AcceptorCallback {
    virtual void process_proposal(uint64_t proposal_number, const L &proposed_value, uint64_t proposer_id) = 0;
};

template<typename L>
struct ProposerCallback {
    virtual void process_ack(uint64_t proposal_number) = 0;

    virtual void process_nack(uint64_t proposal_number, L &value) = 0;
};

enum Recipient : uint8_t {
    ToAcceptor = 0,
    ToProposer = 1
};

template<typename L>
struct FaleiroProtocol {

    TcpServer server;
    ThreadPool<std::tuple<int, AcceptorCallback<L> *, ProposerCallback<L> *>> clients_processors;

    std::unordered_map<uint64_t, ProcessDescriptor> descriptors;

    std::atomic<bool> should_stop = false;

    explicit FaleiroProtocol(uint64_t port) : server(port), clients_processors(
            [&](std::tuple<int, AcceptorCallback<L> *, ProposerCallback<L> *> &val) { this->process_client(val); }, 3),
                                              thread_pool(
                                                      [&](std::pair<uint64_t, AcceptorResponse<L>> &data) {
                                                          this->send_response_pool(data);
                                                      }, 3) {}

    ThreadPool<std::pair<uint64_t, AcceptorResponse<L>>> thread_pool;

    void send_response(uint64_t to, AcceptorResponse<L> &response) {
        thread_pool.add_job({to, response});
    }

    void send_response_pool(const std::pair<uint64_t, AcceptorResponse<L>> &data) {
        uint64_t to = data.first;
        auto response = data.second;
        uint8_t isAck = std::holds_alternative<Ack<L>>(response);
        int sock = open_socket(descriptors.at(to));
        send_byte(sock, ToProposer);
        if (isAck) {
            LOG(INFO) << ">> sending ack to" << to;
            Ack<L> res = std::get<Ack<L>>(response);
            send_byte(sock, isAck);
            send_number(sock, res.proposal_number);
            send_number(sock, res.proposer_id);
            send_lattice(sock, res.proposed_value);
        } else {
            LOG(INFO) << ">> sending nack to" << to;
            Nack<L> res = std::get<Nack<L>>(response);
            send_byte(sock, isAck);
            send_number(sock, res.proposal_number);
            send_number(sock, res.proposer_id);
            send_lattice(sock, res.proposed_value);
        }
    }

    void send_proposal(const L &proposed_value, uint64_t proposal_number, uint64_t proposer_id) {
        std::thread([&, proposed_value, proposal_number, proposer_id]() {
            for (const auto& peer: descriptors) {
                try {
                    LOG(INFO) << ">> sending propose to" << peer.first;
                    int sock = open_socket(peer.second);
                    send_byte(sock, ToAcceptor);
                    send_number(sock, proposal_number);
                    send_lattice(sock, proposed_value);
                    send_number(sock, proposer_id);
                    close(sock);
                } catch (std::runtime_error &e) {
                    LOG(ERROR) << "* Exception while send_proposal" << e.what();
                }
            }
        }).detach();
    }

    void start(AcceptorCallback<L> *acceptorCallback, ProposerCallback<L> *proposerCallback) {
        std::thread(
                [&, acceptorCallback, proposerCallback]() {
                    while (!should_stop) {
                        int socket = server.accept_client();
                        clients_processors.add_job({socket, acceptorCallback, proposerCallback});
                    }
                }).detach();
    }

    void process_client(std::tuple<int, AcceptorCallback<L> *, ProposerCallback<L> *> &val) {
        int sock = std::get<0>(val);
        auto acceptorCallback = std::get<1>(val);
        auto proposerCallback = std::get<2>(val);

        uint8_t message_type = read_byte(sock);
        if (message_type == ToAcceptor) {
            uint64_t proposal_number = read_number(sock);
            L proposed_value = read_lattice<L>(sock);
            uint64_t proposer_id = read_number(sock);
            acceptorCallback->process_proposal(proposal_number, proposed_value, proposer_id);
        } else if (message_type == ToProposer) {
            uint8_t isAck = read_byte(sock);
            uint64_t proposal_number = read_number(sock);
            uint64_t _proposer_id = read_number(sock);
            L lattice = read_lattice<L>(sock);
            if (isAck == 1) {
                proposerCallback->process_ack(proposal_number);
            } else if (isAck == 0) {
                proposerCallback->process_nack(proposal_number, lattice);
            } else {
                LOG(ERROR) << "Wrong isAck" << (int)isAck;
                throw std::runtime_error("wrong isAck");
            }
        } else {
            LOG(ERROR) << "Unknown message type:" << (int)message_type;
            throw std::runtime_error("unknown message type");
        }
        close(sock);
    }

    void add_process(const ProcessDescriptor &descriptor) {
        descriptors[descriptor.id] = descriptor;
    }

    void stop() {
        should_stop = true;
    }
};