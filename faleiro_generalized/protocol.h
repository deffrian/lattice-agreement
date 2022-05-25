#pragma once

#include <variant>
#include <vector>

#include "general/net/server.h"

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
struct FaleiroProtocol : net::IMessageReceivedCallback {

    net::Server server;

    std::unordered_map<uint64_t, net::ProcessDescriptor> descriptors;

    explicit FaleiroProtocol(uint64_t port) : server(this, port) {}

    // Send ack to proposer and to all learners
    void send_response(uint64_t to, AcceptorResponse<L> &response) {
        uint8_t isAck = std::holds_alternative<Ack<L>>(response);
        {
            net::Message message;
            message << ToProposer;
            if (isAck) {
                LOG(INFO) << ">> sending ack to proposer" << to;
                Ack<L> res = std::get<Ack<L>>(response);
                message << Accept << res.proposal_number << res.proposer_id << res.proposed_value;
            } else {
                Nack<L> res = std::get<Nack<L>>(response);
                LOG(INFO) << ">> sending nack to proposer" << to << res.proposed_value;
                message << NAccept << res.proposal_number << res.proposer_id << res.proposed_value;
            }
            server.send(descriptors.at(to), message);
        }

        // Send ack to all learners
        if (isAck) {
            Ack<L> res = std::get<Ack<L>>(response);
            net::Message message;
            message << ToLearner << res.proposal_number << res.proposed_value << res.proposer_id;
            for (const auto &peer : descriptors) {
                LOG(INFO) << ">> sending ack to learner" << to;
                server.send(peer.second, message);
            }
        }
    }

    void send_internal_receive(const L& value, uint64_t except) {
        net::Message message;
        message << ToProposer << InternalReceive << value;
        for (const auto &peer: descriptors) {
            if (peer.first == except) continue;
            LOG(INFO) << ">> send internal receive to" << peer.first;
            server.send(peer.second, message);
        }
    }

    void send_proposal(const L &proposed_value, uint64_t proposal_number, uint64_t proposer_id) {
        std::thread([&, proposed_value, proposal_number, proposer_id]() {
            net::Message message;
            message << ToAcceptor << proposal_number << proposed_value << proposer_id;
            for (const auto& peer: descriptors) {
                try {
                    LOG(INFO) << ">> sending propose to" << peer.first;
//                    int sock = open_socket(peer.second);
//                    send_byte(sock, ToAcceptor);
//                    send_number(sock, proposal_number);
//                    send_lattice(sock, proposed_value);
//                    send_number(sock, proposer_id);
//                    close(sock);
                    server.send(peer.second, message);
                } catch (std::runtime_error &e) {
                    LOG(ERROR) << "* Exception while send_proposal" << e.what();
                }
            }
        }).detach();
    }

    AcceptorCallback<L> *acceptor_callback;
    ProposerCallback<L> *proposer_callback;
    LearnerCallback<L> *learner_callback;

    void start(AcceptorCallback<L> *acceptorCallback, ProposerCallback<L> *proposerCallback, LearnerCallback<L> *learnerCallback) {
        acceptor_callback = acceptorCallback;
        proposer_callback = proposerCallback;
        learner_callback = learnerCallback;
        server.start();
    }

    void on_message_received(net::Message &message) override {
        uint8_t destination;
        message >> destination;
        if (destination == ToAcceptor) {
            uint64_t proposal_number;
            L proposed_value;
            uint64_t proposer_id;
            message >> proposal_number >> proposed_value >> proposer_id;
            acceptor_callback->process_proposal(proposal_number, proposed_value, proposer_id);
        } else if (destination == ToProposer) {
            uint8_t message_type;
            message >> message_type;
            if (message_type == Accept) {
                uint64_t proposal_number;
                uint64_t proposer_id;
                L lattice;
                message >> proposal_number >> proposer_id >> lattice;
                proposer_callback->process_ack(proposal_number);
            } else if (message_type == NAccept) {
                uint64_t proposal_number;
                uint64_t proposer_id;
                L lattice;
                message >> proposal_number >> proposer_id >> lattice;
                proposer_callback->process_nack(proposal_number, lattice);
            } else if (message_type == InternalReceive) {
                L lattice;
                message >> lattice;
                proposer_callback->process_internal_receive(lattice);
            } else {
                LOG(ERROR) << "Wrong message_type" << (int)message_type;
                throw std::runtime_error("wrong message_type");
            }
        } else if (destination == ToLearner) {
            uint64_t proposal_number;
            L lattice;
            uint64_t proposer_id;
            message >> proposal_number >> lattice >> proposer_id;
            learner_callback->process_ack(proposal_number, lattice, proposer_id);
        } else {
            LOG(ERROR) << "Unknown message type:" << (int)destination;
            throw std::runtime_error("unknown message type");
        }
    }

    void add_process(const net::ProcessDescriptor &descriptor) {
        descriptors[descriptor.id] = descriptor;
    }

    void stop() {
        server.stop();
    }
};