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
    virtual void process_internal_receive(const L &value) = 0;
};

template<typename L>
struct LearnerCallback {
    virtual void process_ack(uint64_t proposal_number, const L &value, uint64_t proposer_id) = 0;
};

enum Recipient : uint8_t {
    ToAcceptor = 0,
    ToProposer = 1,
    ToLearner = 2
};

enum MessageType : uint8_t {
    Accept = 0,
    NAccept = 1,
    InternalReceive = 2
};

template<typename L>
struct FaleiroProtocol {

    TcpServer server;
    ThreadPool<std::tuple<int, AcceptorCallback<L> *, ProposerCallback<L> *, LearnerCallback<L> *>> clients_processors;

    std::unordered_map<uint64_t, ProcessDescriptor> descriptors;

    std::atomic<bool> should_stop = false;

    explicit FaleiroProtocol(uint64_t port) : server(port),
                                              clients_processors(
                                                      [&](std::tuple<int, AcceptorCallback<L> *, ProposerCallback<L> *, LearnerCallback<L> *> &val) {
                                                          this->process_client(val);
                                                      }, 3),
                                              acceptor_pool(
                                                      [&](std::pair<uint64_t, AcceptorResponse<L>> &data) {
                                                          this->acceptor_pool_processor(data);
                                                      }, 3),
                                              receive_pool([&](std::tuple<uint64_t, L> &data) {
                                                  this->receive_pool_processor(data);
                                              }, 3) {}

    ThreadPool<std::pair<uint64_t, AcceptorResponse<L>>> acceptor_pool;

    // Send ack to proposer and to all learners
    void send_response(uint64_t to, AcceptorResponse<L> &response) {
        acceptor_pool.add_job({to, response});
    }

    void acceptor_pool_processor(const std::pair<uint64_t, AcceptorResponse<L>> &data) {
        uint64_t to = data.first;
        auto response = data.second;
        uint8_t isAck = std::holds_alternative<Ack<L>>(response);
        {
            int sock = open_socket(descriptors.at(to));
            send_byte(sock, ToProposer);
            if (isAck) {
                LOG(INFO) << ">> sending ack to proposer" << to;
                Ack<L> res = std::get<Ack<L>>(response);
                send_byte(sock, Accept);
                send_number(sock, res.proposal_number);
                send_number(sock, res.proposer_id);
                send_lattice(sock, res.proposed_value);
            } else {
                Nack<L> res = std::get<Nack<L>>(response);
                LOG(INFO) << ">> sending nack to proposer" << to << res.proposed_value;
                send_byte(sock, NAccept);
                send_number(sock, res.proposal_number);
                send_number(sock, res.proposer_id);
                send_lattice(sock, res.proposed_value);
            }
        }

        // Send ack to all learners
        if (isAck) {
            for (const auto &peer : descriptors) {
                Ack<L> res = std::get<Ack<L>>(response);
                int lerner_sock = open_socket(peer.second);
                LOG(INFO) << ">> sending ack to learner" << to;
                send_byte(lerner_sock, ToLearner);
                send_number(lerner_sock, res.proposal_number);
                send_lattice(lerner_sock, res.proposed_value);
                send_number(lerner_sock, res.proposer_id);
            }
        }
    }

    ThreadPool<std::tuple<uint64_t, L>> receive_pool;

    void send_internal_receive(const L& value, uint64_t except) {
        receive_pool.add_job({except, value});
    }

    void receive_pool_processor(const std::tuple<uint64_t, L> &data) {
        uint64_t except = std::get<0>(data);
        L value = std::get<1>(data);
        for (const auto &peer: descriptors) {
            if (peer.first == except) continue;
            LOG(INFO) << ">> send internal receive to" << peer.first;
            int sock = open_socket(peer.second);

            send_byte(sock, ToProposer);
            send_byte(sock, InternalReceive);
            send_lattice(sock, value);
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

    void start(AcceptorCallback<L> *acceptorCallback, ProposerCallback<L> *proposerCallback, LearnerCallback<L> *learnerCallback) {
        std::thread(
                [&, acceptorCallback, proposerCallback, learnerCallback]() {
                    while (!should_stop) {
                        int socket = server.accept_client();
                        clients_processors.add_job({socket, acceptorCallback, proposerCallback, learnerCallback});
                    }
                }).detach();
    }

    void process_client(std::tuple<int, AcceptorCallback<L> *, ProposerCallback<L> *, LearnerCallback<L> *> &val) {
        int sock = std::get<0>(val);
        auto acceptorCallback = std::get<1>(val);
        auto proposerCallback = std::get<2>(val);
        auto learnerCallback = std::get<3>(val);

        uint8_t destination = read_byte(sock);
        if (destination == ToAcceptor) {
            uint64_t proposal_number = read_number(sock);
            L proposed_value = read_lattice<L>(sock);
            uint64_t proposer_id = read_number(sock);
            acceptorCallback->process_proposal(proposal_number, proposed_value, proposer_id);
        } else if (destination == ToProposer) {
            uint8_t message_type = read_byte(sock);
            if (message_type == Accept) {
                uint64_t proposal_number = read_number(sock);
                uint64_t _proposer_id = read_number(sock);
                L lattice = read_lattice<L>(sock);
                proposerCallback->process_ack(proposal_number);
            } else if (message_type == NAccept) {
                uint64_t proposal_number = read_number(sock);
                uint64_t _proposer_id = read_number(sock);
                L lattice = read_lattice<L>(sock);
                proposerCallback->process_nack(proposal_number, lattice);
            } else if (message_type == InternalReceive) {
                L lattice = read_lattice<L>(sock);
                proposerCallback->process_internal_receive(lattice);
            } else {
                LOG(ERROR) << "Wrong message_type" << (int)message_type;
                throw std::runtime_error("wrong message_type");
            }
        } else if(destination == ToLearner) {
            uint64_t proposal_number = read_number(sock);
            L lattice = read_lattice<L>(sock);
            uint64_t proposer_id = read_number(sock);
            learnerCallback->process_ack(proposal_number, lattice, proposer_id);
        } else {
            LOG(ERROR) << "Unknown message type:" << (int)destination;
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