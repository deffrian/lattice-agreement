#pragma once

#include <unordered_set>
#include <memory>
#include <cstddef>
#include <sstream>

/**
 * Set of @uint64_t values. Used as Lattice.
 */
class LatticeSet {
public:
    using Self = LatticeSet;

    LatticeSet() = default;

    LatticeSet(const LatticeSet& other) {
        set = other.set;
    }

    /**
     * Lattice join operator.
     * @param a first set
     * @param b second set
     * @return join of @a and @b
     */
    static Self join(const Self &a, const Self &b) {
        Self result = a;
        for (auto &elem : b.set) {
            result.insert(elem);
        }
        return result;
    }

    /**
     * Lattice less or equal operator.
     * @param other right set
     * @return true when @other contains all number that this contains. false otherwise
     */
    bool operator<=(const Self &other) const {
        for (auto elem : set) {
            if (other.set.count(elem) == 0) {
                return false;
            }
        }
        return true;
    }

    /**
     * Lattice less operator.
     * @param other right set
     * @return true when @other contains all numbers that this contains and this not equal to @other. false otherwise
     */
    bool operator<(const Self &other) const {
        return *this <= other && *this != other;
    }

    /**
     * Lattice equals operator.
     * @param other right set
     * @return true when this contains same values as @other. false otherwise
     */
    bool operator==(const Self &other) const {
        if (set.size() != other.set.size()) return false;
        return *this <= other && other <= *this;
    }

    /**
     * Lattice not equals operator
     * @param other right set
     * @return true when this not equal to @other. false otherwise
     */
    bool operator!=(const Self &other) const {
        return !(*this == other);
    }

    /**
     * Insert number into set
     * @param elem value that will be inserted
     */
    void insert(uint64_t elem) {
        set.insert(elem);
    }

public:

    // actual set
    std::unordered_set<uint64_t> set;
};
