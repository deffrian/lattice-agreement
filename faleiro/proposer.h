#pragma once

#include <optional>
#include <unistd.h>
#include <cassert>
#include <random>

#include "lattice.h"
#include "acceptor.h"
#include "network.h"
#include "lattice_agreement.h"

enum Status {
    Passive,
    Active,
};

template<typename L, typename P>
struct Proposer : LatticeAgreement<L> {
    uint64_t uid;
    Status status;
    uint64_t ack_count;
    uint64_t nack_count;
    uint64_t active_proposal_number;
    L proposed_value;
    P &protocol;


    explicit Proposer(P &protocol) : protocol(protocol), status(Passive), ack_count(0), nack_count(0),
                                     active_proposal_number(0) {
        std::random_device dev;
        std::mt19937 mt(dev());
        std::uniform_int_distribution<uint64_t> dist;
        uid = dist(mt);
        std::cout << "proposer id: " << uid << std::endl;
    }

    L start(const L &initial_value) override {
        propose(initial_value);
        while (true) {
            auto result = decide();
            if (result.has_value()) {
                return result.value();
            } else {
                refine();
            }
        }
    }

    uint64_t get_uid() {
        return uid;
    }

    void propose(const L &initial_value) {
        if (active_proposal_number == 0) {
            proposed_value = initial_value;
            status = Status::Active;
            active_proposal_number += 1;
            protocol.send_proposal(ProposerRequest<L>(proposed_value, active_proposal_number, uid), *this);
        }
    }

    void process_ack(uint64_t proposal_number) {
        if (proposal_number == active_proposal_number) {
            ack_count += 1;
        }
    }

    void process_nack(uint64_t proposal_number, L &value) {
        if (proposal_number == active_proposal_number) {
            proposed_value = LatticeSet::join(proposed_value, value);
            nack_count += 1;
        }
    }

    void refine() {
        uint64_t acc_cnt = protocol.get_acceptors_count();
        if (nack_count > 0 && status == Status::Active &&
            nack_count + ack_count >= (acc_cnt + 2) / 2) {
            active_proposal_number += 1;
            ack_count = 0;
            nack_count = 0;
            protocol.send_proposal(ProposerRequest<L>(proposed_value, active_proposal_number, uid), *this);
        }
    }

    std::optional<L> decide() {
        uint64_t acc_cnt = protocol.get_acceptors_count();
        if (ack_count >= (acc_cnt + 2) / 2 && status == Status::Active) {
            status = Status::Passive;
            return {proposed_value};
        } else {
            return {};
        }
    }
};

template<typename L>
struct ProposerProtocolTcp {
    std::vector<ProcessDescriptor> acceptors;

    void add_acceptor(const ProcessDescriptor &acceptor) {
        acceptors.push_back(acceptor);
    }

    void send_proposal(const ProposerRequest<L> &proposal, Proposer<L, ProposerProtocolTcp<L>> &proposer) {
        for (auto & acceptor : acceptors) {
            auto res = get_reply(proposal, acceptor);
            if (std::holds_alternative<Ack<L>>(res)) {
                proposer.process_ack(std::get<Ack<L>>(res).proposal_number);
            } else {
                auto nack = std::get<Nack<L>>(res);
                proposer.process_nack(nack.proposal_number, nack.proposed_value);
            }
        }
    }

    std::variant<Ack<L>, Nack<L>> get_reply(const ProposerRequest<L> &proposal, const ProcessDescriptor &descriptor) {
        int sock = open_socket(descriptor);
        std::cout << "Sending request" << std::endl;
//        send_number(sock, proposal.proposal_number);
        send_number(sock, proposal.proposal_number);
        send_number(sock, proposal.proposer_id);
        send_lattice(sock, proposal.proposed_value);

        std::cout << "Receiving reply" << std::endl;

        uint8_t isAck = -1;
        ssize_t len = read(sock, &isAck, 1);
        if (len != 1) {
            assert(false);
        }
        uint64_t proposal_number = read_number(sock);
        uint64_t proposer_id = read_number(sock);
        auto set_recv = read_lattice<LatticeSet>(sock);
        close(sock);
        if (isAck) {
            return {Ack<L>{proposal_number, set_recv, proposer_id}};
        } else {
            return {Nack<L>{proposal_number, set_recv, proposer_id}};
        }
    }

    [[nodiscard]] uint64_t get_acceptors_count() const {
        return acceptors.size();
    }
};
