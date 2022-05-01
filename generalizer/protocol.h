#pragma once

#include "general/network.h"

template<typename L>
struct CallbackGeneralizer {
    virtual void receive_propose(uint64_t i, uint64_t j, const L &vp) = 0;

    virtual void receive_prepare(uint64_t i, uint64_t j, uint64_t rp) = 0;

    virtual void receive_pre_sched(uint64_t i, uint64_t j, uint64_t sp) = 0;

    virtual void receive_skip() = 0;

    virtual ~CallbackGeneralizer() = default;
};

template<typename L>
struct Protocol {

    TcpServer server;

    explicit Protocol(uint64_t port) : server(port) {

    }

    void start(CallbackGeneralizer<L> *callback) {

    }

    void send_propose(uint64_t j, const L &v) {

    }

    void send_prepare(uint64_t i, uint64_t j, uint64_t l) {

    }

    void send_pre_sched(uint64_t i, uint64_t j, uint64_t mx) {

    }

    void send_skip(uint64_t l, uint64_t from) {

    }

private:
    void process_client(CallbackGeneralizer<L> *callback) {

    }
};