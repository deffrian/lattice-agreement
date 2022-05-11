#pragma once

#include <optional>
#include <unistd.h>
#include <random>

#include "general/lattice_agreement.h"
#include "general/lattice.h"
#include "general/network.h"
#include "acceptor.h"

enum Status {
    Passive,
    Active,
};

template<typename L>
struct Proposer : LatticeAgreement<L>, ProposerCallback<L> {
    uint64_t uid;
    Status status;
    uint64_t ack_count;
    uint64_t nack_count;
    uint64_t active_proposal_number;
    L proposed_value;
    FaleiroProtocol<L> &protocol;

    uint64_t n;

    std::mutex mt;
    std::condition_variable cv;


    Proposer(FaleiroProtocol<L> &protocol, uint64_t uid, uint64_t n) : protocol(protocol), status(Passive),
                                                                       ack_count(0),
                                                                       nack_count(0), n(n),
                                                                       active_proposal_number(0), uid(uid) {}

    L start(const L &initial_value) override {
        propose(initial_value);
        std::unique_lock lk{mt};
        while (true) {
            cv.wait(lk);
            auto result = decide();
            if (result.has_value()) {
                return result.value();
            } else {
                refine();
            }
        }
    }

    void propose(const L &initial_value) {
        if (active_proposal_number == 0) {
            proposed_value = initial_value;
            status = Status::Active;
            active_proposal_number += 1;
            protocol.send_proposal(proposed_value, active_proposal_number, uid);
        }
    }

    void process_ack(uint64_t proposal_number) override {
        std::lock_guard lg{mt};
        if (proposal_number == active_proposal_number) {
            LOG(INFO) << "ack received";
            ack_count += 1;
            cv.notify_one();
        }
    }

    void process_nack(uint64_t proposal_number, L &value) override {
        std::lock_guard lg{mt};
        if (proposal_number == active_proposal_number) {
            LOG(INFO) << "nack received";
            proposed_value = LatticeSet::join(proposed_value, value);
            nack_count += 1;
            cv.notify_one();
        }
    }

    void refine() {
        uint64_t acc_cnt = n;
        if (nack_count > 0 && status == Status::Active &&
            nack_count + ack_count >= (acc_cnt + 2) / 2) {
            active_proposal_number += 1;
            ack_count = 0;
            nack_count = 0;
            protocol.send_proposal(proposed_value, active_proposal_number, uid);
        }
    }

    std::optional<L> decide() {
        uint64_t acc_cnt = n;
        if (ack_count >= (acc_cnt + 2) / 2 && status == Status::Active) {
            status = Status::Passive;
            return {proposed_value};
        } else {
            return {};
        }
    }
};
