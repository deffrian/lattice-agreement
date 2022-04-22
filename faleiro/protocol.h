#pragma once

#include <variant>
#include <vector>

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
struct ProposerRequest {
    ProposerRequest(L &proposed_value, uint64_t proposal_number, uint64_t proposer_id) :
            proposed_value(proposed_value),
            proposal_number(proposal_number),
            proposer_id(proposer_id) {}

    L proposed_value;
    uint64_t proposal_number;
    uint64_t proposer_id;
};

