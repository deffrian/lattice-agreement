#pragma once

#include <unordered_set>
#include <set>
#include "general/generalized_lattice_agreement.h"
#include "protocol.h"

template<typename L, typename LAGenerator>
struct Generalizer : GeneralizedLA<L>, CallbackGeneralizer<L> {

    uint64_t N;
    uint64_t f;
    Protocol<L> &protocol;
    LAGenerator &la_gen;

    // index of current proposal
    int64_t j = -1;

    // last instance of LA known to have output a value
    int64_t l = -1;

    // Values pending to be scheduled
    std::set<std::tuple<uint64_t, uint64_t, uint64_t>> P;

    // Set of scheduled inputs
    std::set<std::tuple<uint64_t, uint64_t, uint64_t>> I;

    // Set of proposed inputs
    std::set<std::tuple<uint64_t, uint64_t, uint64_t>> Ip;

    // Last accepted value
    L a;

    // Next accepted value
    L ap;

    // v[i][j] corresponds to j-th input of c_i
    std::vector<std::vector<L>> v;

    // r[i][j] is a set or prepare values for v_i_j
    std::vector<std::vector<std::set<uint64_t>>> r;

    // s[i][j] is a set of pre-schedules for v_i_j
    std::vector<std::vector<std::set<uint64_t>>> s;

    explicit Generalizer(Protocol<L> &protocol, LAGenerator &la_gen, uint64_t N, uint64_t f) : protocol(protocol), N(N), f(f), la_gen(la_gen) {}

    void start() override {
        bool exists = false;
        for (auto elem: P) {}

        if (!exists && !I.empty()) {
            Ip.clear();
            std::copy_if(I.begin(), I.end(), std::inserter(Ip, Ip.end()),
                         [=](std::tuple<uint64_t, uint64_t, uint64_t> e) {
                             return std::get<0>(e) <= l + 1;
                         });
            L prop = a;
            for (auto elem: Ip) {
                auto i_cur = std::get<1>(elem);
                auto j_cur = std::get<2>(elem);
                prop = L::join(prop, v[i_cur][j_cur]);
            }
            ap = la_gen.propose_to(l + 1, prop);
        }

    }

    L propose(const L &x) override {
        ++j;
        protocol.send_propose(j, x);
    }

    void receive_propose(uint64_t i, uint64_t j_rec, const L &vp) override {
        v[i][j_rec] = vp;
        P.insert({l + 2, i, j_rec});
        protocol.send_prepare(i, j_rec, l + 2);
    }

    void receive_prepare(uint64_t i, uint64_t j_rec, uint64_t rp) override {
        r[i][j_rec].insert(rp);
        if (r[i][j_rec].size() == N - f && v[i][j_rec].set.empty()) {
            protocol.send_pre_sched(i, j_rec, *r[i][j_rec].rbegin());
        }
    }

    void receive_pre_sched(uint64_t i, uint64_t j_rec, uint64_t sp) override {
        s[i][j_rec].insert(sp);
    }

    void receive_skip() override {

    }
};
