#pragma once

#include "zheng_la.h"

template<typename L>
struct ZhengLAGenerator {

    std::map<uint64_t, ZhengLA<L>*> active;
    std::set<uint64_t> terminated;

    L propose_to(uint64_t idx, const L &prop) {
        if (terminated.contains(idx)) {
            throw std::runtime_error();
        }

        return active[idx]->start(prop);
    }
};