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
    double l;
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
    std::unique_lock<std::mutex> lk{cv_m};

    ZhengLA(uint64_t f, uint64_t n, uint64_t i, ProtocolTcp<L> &protocol) : f(f), n(n), i(i), protocol(protocol), v(n) {
        l = (double)n - (double) f / 2.;
        log_f = std::ceil(std::log2(f));
        acceptVal.resize(log_f + 1);
    }

    enum Class {
        Master,
        Slave
    };

    L start(const L &x) override {

        v[i] = x;

        protocol.send_value(v, i);
        LOG(INFO) << "Waiting for values";
        cv.wait(lk, [&] {
//            LOG(ERROR) << "CHECK";
            return value_received >= n - f;
        });
        LOG(INFO) << "All values received ";

        double delta = f / 2.;
        for (r = 1; r <= log_f; ++r) {
            LOG(INFO) << "classifier iteration: " << r;
            Class c = classifier(l);
            delta /= 2.;
            if (c == Master) {
                v = w;
                l = l + delta;
            } else {
                l = l - delta;
            }
            LOG(INFO) << "classifier iteration done: " << r;
        }

        L y;
        for (size_t j = 0; j < n; ++j) {
            y = L::join(y, v[j]);
        }
        lk.unlock();
        return y;
    }


    std::vector<L> w;
    bool build_w = false;
    bool build_wp = false;

    using AcceptValT = std::vector<std::pair<std::vector<L>, double>>;
    std::vector<AcceptValT> acceptVal;


    Class classifier(double k) {
        w.assign(n, L{});

        LOG(INFO) << "Waiting for send ack";
        protocol.send_write(v, k, r, i);
        while (write_ack_received < n - f) cv.wait(lk);
        write_ack_received = 0;
        LOG(INFO) << "Done waiting for send ack";


        protocol.send_read(r, i);
        build_w = true;
        while (read_ack_received < n - f) cv.wait(lk);
        read_ack_received = 0;
        build_w = false;

        uint64_t h = 0;
        for (size_t j = 0; j < n; ++j) {
            if (!w[j].set.empty()) {
                ++h;
            }
        }

        if ((double)h > k) {
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
        cv_m.lock();
        LOG(INFO) << "<< write ack received" << message_id << (rec_r == r);
        if (rec_r == r) {
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
        }
        cv.notify_all();
        cv_m.unlock();
    }

    void receive_read_ack(const AcceptValT &recVal, uint64_t rec_r, uint64_t message_id) override {
        cv_m.lock();
        LOG(INFO) << "<< read ack received" << message_id << (rec_r == r) << build_w;
        if (rec_r == r && build_w) {
//            std::cout << "locked" << std::endl;
            for (auto &elem: recVal) {
                if (elem.second == l) {
                    for (size_t j = 0; j < n; ++j) {
                        w[j] = L::join(w[j], elem.first[j]);
                    }
                }
            }
            read_ack_received++;
        }
        cv.notify_all();
        cv_m.unlock();
    }

    void receive_value(const std::vector<L> &value, uint64_t message_id) override {
        cv_m.lock();
//        LOG(ERROR) << "<< value received" << message_id;
        if (value_received < n - f) {
            for (size_t k = 0; k < n; ++k) {
                v[k] = L::join(v[k], value[k]);
            }
            value_received++;
        }
        cv.notify_all();
        cv_m.unlock();
    }

    void receive_write(const std::vector<L> &value, double k, uint64_t rec_r, uint64_t from, uint64_t message_id) override {
        cv_m.lock();
        LOG(INFO) << "<< write received from " << from << " message id " << message_id;

        if (!acceptValContains(acceptVal[rec_r], k, value)) {
            acceptVal[rec_r].emplace_back(value, k);
        }
        auto copy = acceptVal[rec_r];
        cv_m.unlock();
        protocol.send_write_ack(from, copy, rec_r, i, message_id);
    }

    bool acceptValContains(const AcceptValT &recVal, double k, const std::vector<L> &value) {
        for (size_t j = 0; j < recVal.size(); ++j) {
            if (recVal[j].second == k && recVal[j].first == value) {
                return true;
            }
        }
        return false;
    }

    void receive_read(uint64_t rec_r, uint64_t from, uint64_t message_id) override {
        cv_m.lock();
        LOG(INFO) << "<< read received from " << from << "message id" << message_id;
        auto copy = acceptVal[rec_r];
        cv_m.unlock();
        protocol.send_read_ack(from, copy, rec_r, i, message_id);
    }
};
