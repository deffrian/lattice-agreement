#pragma once

#include <optional>

#include "lattice.h"
#include "acceptor.h"

enum Status {
    Passive,
    Active,
};

template<typename L, typename P>
struct Proposer {
    uint64_t uid;
    Status status;
    uint64_t ack_count;
    uint64_t nack_count;
    uint64_t active_proposal_number;
    L proposed_value;
    P &protocol;


    explicit Proposer(P &protocol) : protocol(protocol), status(Passive), ack_count(0), nack_count(0),
                                     active_proposal_number(0) {
        uid = rand();
    }

    L get_value(L &initial_value) {
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

    void propose(L &initial_value) {
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
    std::vector<AcceptorDescriptor> acceptors;

    void add_acceptor(const AcceptorDescriptor &acceptor) {
        acceptors.push_back(acceptor);
    }

    void send_proposal(const ProposerRequest<L> &proposal, Proposer<L, ProposerProtocolTcp<L>> &proposer) {
        std::cout << proposal.proposer_id << std::endl;


        for (size_t i = 0; i < g_acceptors.size(); ++i) {
            auto res = g_acceptors[i]->process_proposal(proposal.proposal_number, proposal.proposed_value,
                                                        proposal.proposer_id);
            if (std::holds_alternative<Ack<L>>(res)) {
                proposer.process_ack(std::get<Ack<L>>(res).proposal_number);
            } else {
                auto nack = std::get<Nack<L>>(res);
                proposer.process_nack(nack.proposal_number, nack.proposed_value);
            }
        }
    }

    uint64_t get_acceptors_count() const {
        return acceptors.size();
    }
};
