#pragma once

template<typename L>
struct GeneralizedLA {
    virtual void start() = 0;
    virtual L propose(const L &x) = 0;
    virtual ~GeneralizedLA() = default;
};
