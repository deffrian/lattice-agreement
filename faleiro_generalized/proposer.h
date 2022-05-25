#pragma once

#include <optional>
#include <unistd.h>
#include <random>

#include "general/lattice_agreement.h"
#include "general/lattice.h"
#include "acceptor.h"

enum Status {
    Passive,
    Active,
};

template<typename L>
struct Proposer : ProposerCallback<L> {
    uint64_t uid;
    Status status;
    uint64_t ack_count;
    uint64_t nack_count;
    uint64_t active_proposal_number;
    L proposed_value;
    L buffered_values;

    FaleiroProtocol<L> &protocol;

    uint64_t n;

    std::mutex mt;
    std::condition_variable cv;


    Proposer(FaleiroProtocol<L> &protocol, uint64_t uid, uint64_t n) : protocol(protocol), status(Passive),
                                                                       ack_count(0),
                                                                       nack_count(0), n(n),
                                                                       active_proposal_number(0), uid(uid) {}

    L start() {
        std::unique_lock lk{mt};
        propose();
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

    void propose() {
        auto tmp = L::join(proposed_value, buffered_values);
        if (status == Passive && proposed_value < tmp) {
            proposed_value = tmp;
            status = Status::Active;
            active_proposal_number += 1;
            ack_count = 0;
            nack_count = 0;
            protocol.send_proposal(proposed_value, active_proposal_number, uid);
            buffered_values = L();
        }
    }

    // Buffer
    void process_internal_receive(const L &value) override {
        std::lock_guard lg{mt};
        buffered_values = L::join(value, buffered_values);
    }

    void process_ack(uint64_t proposal_number) override {
        std::lock_guard lg{mt};
        if (proposal_number == active_proposal_number) {
            ack_count += 1;
            cv.notify_one();
        }
    }

    void process_nack(uint64_t proposal_number, L &value) override {
        std::lock_guard lg{mt};
        if (proposal_number == active_proposal_number) {
            proposed_value = LatticeSet::join(proposed_value, value);
            nack_count += 1;
            cv.notify_one();
        }
    }

    void receive_value(const L &value) {
        std::lock_guard lg{mt};
        protocol.send_internal_receive(value, uid);
        buffered_values = L::join(value, buffered_values);
        LOG(INFO) << "buffered_values" << buffered_values;
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
            status = Passive;
            return {proposed_value};
        } else {
            return {};
        }
    }
};
