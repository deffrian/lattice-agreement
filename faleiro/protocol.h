#pragma once

#include <variant>
#include <vector>
#include <atomic>

#include <asio.hpp>

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
};

enum Recipient : uint8_t {
    ToAcceptor = 0,
    ToProposer = 1
};

template<typename L>
struct FaleiroProtocol : net::IMessageReceivedCallback {

    std::unordered_map<uint64_t, net::ProcessDescriptor> descriptors;

    std::atomic<bool> should_stop = false;

    AcceptorCallback<L> *acceptor_callback;
    ProposerCallback<L> *proposer_callback;

    net::Server server;

    explicit FaleiroProtocol(uint64_t port) : server(this, port) {

    }

    void send_response(uint64_t to, AcceptorResponse<L> &response) {
        uint8_t isAck = std::holds_alternative<Ack<L>>(response);
        net::Message message;
        message << ToProposer;
        if (isAck) {
            LOG(INFO) << ">> sending ack to" << to;
            Ack<L> res = std::get<Ack<L>>(response);
            message << isAck << res.proposal_number << res.proposer_id << res.proposed_value;
        } else {
            LOG(INFO) << ">> sending nack to" << to;
            Nack<L> res = std::get<Nack<L>>(response);
            message << isAck << res.proposal_number << res.proposer_id << res.proposed_value;
        }
        server.send(descriptors.at(to), message);
    }

    void send_proposal(const L &proposed_value, uint64_t proposal_number, uint64_t proposer_id) {
        std::thread([&, proposed_value, proposal_number, proposer_id]() {
            for (const auto& peer: descriptors) {
                try {
                    LOG(INFO) << ">> sending propose to" << peer.first;
                    net::Message message;
                    message << ToAcceptor << proposal_number << proposed_value << proposer_id;
                    server.send(peer.second, message);
                } catch (std::runtime_error &e) {
                    LOG(ERROR) << "* Exception while send_proposal" << e.what();
                }
            }
        }).detach();
    }

    void start(AcceptorCallback<L> *acceptorCallback, ProposerCallback<L> *proposerCallback) {
        acceptor_callback = acceptorCallback;
        proposer_callback = proposerCallback;
        server.start();
    }

    void on_message_received(net::Message &message) override {
        uint8_t message_type;
        message >> message_type;
        if (message_type == ToAcceptor) {
            uint64_t proposal_number;
            L proposed_value;
            uint64_t proposer_id;
            message >> proposal_number >> proposed_value >> proposer_id;
            LOG(INFO ) << "message" << "acceptor" << proposal_number << proposed_value << proposer_id;
            acceptor_callback->process_proposal(proposal_number, proposed_value, proposer_id);
        } else if (message_type == ToProposer) {
            uint8_t isAck;
            uint64_t proposal_number;
            uint64_t _proposer_id;
            L lattice;
            message >> isAck >> proposal_number >> _proposer_id >> lattice;
            LOG(INFO ) << "message" << "proposer" << isAck << proposal_number << _proposer_id;
            if (isAck == 1) {
                proposer_callback->process_ack(proposal_number);
            } else if (isAck == 0) {
                proposer_callback->process_nack(proposal_number, lattice);
            } else {
                LOG(ERROR) << "Wrong isAck" << (int)isAck;
                throw std::runtime_error("wrong isAck");
            }
        } else {
            LOG(ERROR) << "Unknown message type:" << (int)message_type;
            throw std::runtime_error("unknown message type");
        }
    }

    void add_process(const net::ProcessDescriptor &descriptor) {
        descriptors[descriptor.id] = descriptor;
    }

    void stop() {
        should_stop = true;
        server.stop();
    }
};