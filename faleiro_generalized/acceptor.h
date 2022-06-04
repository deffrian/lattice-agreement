#pragma once

#include "general/lattice.h"
#include "general/network.h"
#include "protocol.h"

template<typename L>
struct Acceptor : AcceptorCallback<L> {
    L accepted_value;

    FaleiroProtocol<L> &protocol;

    std::mutex mt;

    explicit Acceptor(FaleiroProtocol<L> &protocol) : protocol(protocol) {}

    void process_proposal(uint64_t proposal_number, const L &proposed_value, uint64_t proposer_id) override {
        std::lock_guard lg{mt};
        LOG(INFO) << "<< propose received from" << proposer_id << proposed_value;
        if (accepted_value <= proposed_value) {
            auto res = accept(proposal_number, proposed_value, proposer_id);
            protocol.send_response(proposer_id, res);
        } else {
            auto res = reject(proposal_number, proposed_value, proposer_id);
            protocol.send_response(proposer_id, res);
        }
    }

    // guard: ?Proposal(proposalNumber, proposedValue, proposerId) && acceptedValue <= proposedValue
    AcceptorResponse<L> accept(uint64_t proposal_number, const L &proposed_value, uint64_t proposer_id) {
        accepted_value = proposed_value;
        return Ack{proposal_number, proposed_value, proposer_id};
    }

    // guard: ?Proposal(proposalNumber, proposedValue, proposerId) && acceptedValue !<= proposedValue
    AcceptorResponse<L> reject(uint64_t proposal_number, const L &proposed_value, uint64_t proposer_id) {
        accepted_value = L::join(accepted_value, proposed_value);
        return Nack{proposal_number, accepted_value, proposer_id};
    }
};
