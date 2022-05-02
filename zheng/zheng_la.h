#pragma once

#include <cmath>
#include <vector>
#include <cstdint>
#include <condition_variable>

#include "general/lattice_agreement.h"
#include "general/lattice.h"

#include "protocol.h"

template<typename L>
struct ZhengLA : LatticeAgreement<L>, Callback<L> {

    const uint64_t f;
    const uint64_t n;
    uint64_t l;
    uint64_t i;
    uint64_t log_f;
    uint64_t r;
    ProtocolTcp<L> &protocol;
    std::vector<L> v;

    uint64_t value_received = 0;
    uint64_t read_ack_received = 0;
    uint64_t write_ack_received = 0;

    std::condition_variable cv;
    std::mutex cv_m;

    ZhengLA(uint64_t f, uint64_t n, uint64_t i, ProtocolTcp<L> &protocol) : f(f), n(n), i(i), protocol(protocol), v(n) {
        l = n - f / 2;
        log_f = std::ceil(std::log2(f));
        acceptVal.resize(log_f + 1);
    }

    enum Class {
        Master,
        Slave
    };

    L start(const L &x) override {
        v[i] = x;

        std::unique_lock<std::mutex> lk{cv_m};

        protocol.send_value(v, i);
        std::cout << "Waiting for values" << std::endl;
        while (value_received < n - f) cv.wait(lk);
        std::cout << "All values received " << std::endl;

        uint64_t delta = f / 2;
        for (r = 1; r <= log_f; ++r) {
            std::cout << "classifier iteration: " << r << std::endl;
            Class c = classifier(l, lk);
            delta /= 2;
            if (c == Master) {
                v = w;
                l = l + delta;
            } else {
                l = l - delta;
            }
            std::cout << "classifier iteration done: " << r << std::endl;
        }

        L y;
        for (size_t j = 0; j < n; ++j) {
            y = L::join(y, v[j]);
        }
        return y;
    }


    std::vector<L> w;
    bool build_w = false;
    bool build_wp = false;

    using AcceptValT = std::vector<std::pair<std::vector<L>, uint64_t>>;
    std::vector<AcceptValT> acceptVal;


    Class classifier(uint64_t k, std::unique_lock<std::mutex> &lk) {
        w.assign(n, L{});

        std::cout << "Waiting for send ack" << std::endl;
        protocol.send_write(v, k, r, i);
        while (write_ack_received < n - f) cv.wait(lk);
        std::cout << "Done waiting for send ack" << std::endl;
        write_ack_received = 0;


        build_w = true;
        protocol.send_read(r, i);
        while (read_ack_received < n - f) cv.wait(lk);
        read_ack_received = 0;
        build_w = false;

        uint64_t h = 0;
        for (size_t j = 0; j < n; ++j) {
            if (!w[j].set.empty()) {
                ++h;
            }
        }

        if (h > k) {
            build_wp = true;
            protocol.send_write(w, k, r, i);
            while (write_ack_received < n - f) cv.wait(lk);
            write_ack_received = 0;
            build_wp = false;
            return Master;
        } else {
            return Slave;
        }
    }

    void receive_write_ack(const AcceptValT &recVal, uint64_t rec_r, uint64_t message_id) override {
        if (rec_r == r) {
            std::lock_guard lockGuard{cv_m};
            std::cout << "<< write ack received" << std::endl;
//            std::cout << "locked" << std::endl;
            write_ack_received++;
            if (build_wp) {
                for (auto &elem: recVal) {
                    if (elem.second == l) {
                        for (size_t j = 0; j < n; ++j) {
                            w[j] = L::join(w[j], elem.first[j]);
                        }
                    }
                }
            }
            cv.notify_one();
        }
    }

    void receive_read_ack(const AcceptValT &recVal, uint64_t rec_r, uint64_t message_id) override {
        if (rec_r == r && build_w) {
            std::lock_guard lockGuard{cv_m};
            std::cout << "<< read ack received" << std::endl;
//            std::cout << "locked" << std::endl;
            for (auto &elem: recVal) {
                if (elem.second == l) {
                    for (size_t j = 0; j < n; ++j) {
                        w[j] = L::join(w[j], elem.first[j]);
                    }
                }
            }
            read_ack_received++;
            cv.notify_one();
        }
    }

    void receive_value(const std::vector<L> &value, uint64_t message_id) override {
        std::lock_guard lockGuard{cv_m};
        std::cout << "<< value received" << std::endl;
        value_received++;
        for (size_t k = 0; k < n; ++k) {
            v[k] = L::join(v[k], value[k]);
        }
        cv.notify_one();
    }

    void receive_write(const std::vector<L> &value, uint64_t k, uint64_t rec_r, uint64_t from, uint64_t message_id) override {
        std::lock_guard lockGuard{cv_m};
        std::cout << "<< write received from " << from << " message id " << message_id << std::endl;
        acceptVal[rec_r].emplace_back(value, k);
        protocol.send_write_ack(from, acceptVal[rec_r], rec_r, i, message_id);
    }

    void receive_read(uint64_t rec_r, uint64_t from, uint64_t message_id) override {
        std::lock_guard lockGuard{cv_m};
        std::cout << "<< read received from " << from << std::endl;
//        std::cout << "locked" << std::endl;
        protocol.send_read_ack(from, acceptVal[rec_r], rec_r, i, message_id);
    }
};
