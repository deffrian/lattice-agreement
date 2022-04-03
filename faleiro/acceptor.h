#pragma once

#include "lattice.h"
#include "protocol.h"

template<typename L, typename P>
struct Acceptor {
    L accepted_value;
    uint64_t uid;
    P protocol;

    explicit Acceptor(P &protocol) : protocol(protocol) {
//        uid = rand();
        uid = 0;
    }

    uint64_t get_id() const {
        return uid;
    }

    void start() {
        protocol.start(*this);
    }

    AcceptorResponse<L> process_proposal(uint64_t proposal_number, const L &proposed_value, uint64_t proposer_id) {
        if (accepted_value <= proposed_value) {
            return accept(proposal_number, proposed_value, proposer_id);
        } else {
            return reject(proposal_number, proposed_value, proposer_id);
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

template<typename L>
struct AcceptorProtocolTcp {
    void start(Acceptor<L, AcceptorProtocolTcp> &acceptor) {

    }
};

std::vector<Acceptor<LatticeSet, AcceptorProtocolTcp<LatticeSet>>*> g_acceptors;