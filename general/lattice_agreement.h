#pragma once

template<typename L>
struct LatticeAgreement {
    virtual L start(const L &x) = 0;
    virtual ~LatticeAgreement() = default;
};

