#pragma once

#include <vector>
#include <map>

template<typename L>
struct Learner : LearnerCallback<L> {
    uint64_t n;
    L learnt_value;
    std::vector<std::map<uint64_t, uint64_t>> ack_count;
    FaleiroProtocol<L> &protocol;

    std::mutex mt;
    std::condition_variable cv;

    Learner(FaleiroProtocol<L> &protocol, uint64_t n) : ack_count(n), protocol(protocol), n(n) {}

    void process_ack(uint64_t proposal_number, const L &value, uint64_t proposer_id) override {
        std::lock_guard lg{mt};
        learn(proposal_number, value, proposer_id);
    }

    void learn(uint64_t proposal_number, const L &value, uint64_t proposer_id) {
        ack_count[proposer_id][proposal_number]++;
        LOG(INFO) << "learn" << value << proposal_number << proposer_id;
        if (ack_count[proposer_id][proposal_number] >= (n + 2) / 2 && learnt_value < value) {
            learnt_value = value;
            cv.notify_one();
        }
    }

    L learn_value(const L &proposal) {
        std::unique_lock lk{mt};
        while (true) {
            if (proposal <= learnt_value) {
                return learnt_value;
            }
            cv.wait(lk);
        }
    }
};