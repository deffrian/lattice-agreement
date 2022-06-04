#pragma once

#include <asio.hpp>
#include <utility>

namespace net {
    /**
     * Descriptor of process.
     */
    struct ProcessDescriptor {
        std::string ip_address;
        uint64_t id;
        uint64_t port;
    };
}