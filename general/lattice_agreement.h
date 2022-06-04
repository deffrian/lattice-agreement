#pragma once

/**
 * Interface of Lattice Agreement algorithms.
 * @tparam L Lattice type
 */
template<typename L>
struct LatticeAgreement {
    /**
     * Proposes value. Should be called once.
     * @param x proposed value
     * @return learnt value
     */
    virtual L start(const L &x) = 0;
    virtual ~LatticeAgreement() = default;
};

