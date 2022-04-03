#pragma once

#include <unordered_set>
#include <memory>
#include <cstddef>

class LatticeSet {
public:
    using Self = LatticeSet;

    LatticeSet() = default;

    LatticeSet(const LatticeSet& other) {
        set = other.set;
    }

    static Self join(const Self &a, const Self &b) {
        Self result = a;
        for (auto &elem : b.set) {
            result.insert(elem);
        }
        return result;
    }

    bool operator<=(const Self &other) const {
        for (auto elem : set) {
            if (!other.set.contains(elem)) {
                return false;
            }
        }
        return true;
    }

    void insert(uint64_t elem) {
        set.insert(elem);
    }

public:

    std::unordered_set<uint64_t> set;
};
