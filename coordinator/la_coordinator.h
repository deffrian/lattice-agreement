#pragma once

#include <utility>
#include <vector>

#include "general/network.h"

enum CoordinatorMessage : uint8_t {
    Register = 0,
    TestComplete = 1
};

template<typename L>
struct LACoordinatorClient {

    TcpServer server;
    ProcessDescriptor coordinator_descriptor;

    uint64_t my_id = -1;

    LACoordinatorClient(uint64_t port, ProcessDescriptor coordinator_descriptor) : server(port),
                                                                                   coordinator_descriptor(std::move(
                                                                                           coordinator_descriptor)) {}

    uint64_t send_register(uint64_t protocol_port, const std::string &ip) {
        int sock = open_socket(coordinator_descriptor);
        send_byte(sock, Register);
        send_number(sock, protocol_port);
        send_string(sock, ip);
        my_id = read_number(sock);
        close(sock);
        return my_id;
    }

    void wait_for_test_info(uint64_t &n, uint64_t &f, L &initial_value, std::vector<ProcessDescriptor> &peers) {
        int sock = server.accept_client();
        n = read_number(sock);
        f = read_number(sock);
        initial_value = read_lattice<L>(sock);
        for (uint64_t i = 0; i < n; ++i) {
            uint64_t port = read_number(sock);
            std::string ip = read_string(sock);
            uint64_t id = read_number(sock);
            if (id != my_id) {
                peers.push_back({ip, id, port});
            }
        }
        close(sock);
    }

    void wait_for_start() {
        int sock = server.accept_client();
        close(sock);
    }

    void wait_for_stop() {
        int sock = server.accept_client();
        close(sock);
    }

    void send_test_complete(uint64_t elapsed_time, const L &received_value) {
        int sock = open_socket(coordinator_descriptor);
        send_byte(sock, TestComplete);
        send_number(sock, elapsed_time);
        send_number(sock, my_id);
        send_lattice(sock, received_value);
    }
};

template<typename L>
struct LACoordinator {
    uint64_t n;
    uint64_t f;

    TcpServer server;

    std::vector<ProcessDescriptor> known_peers;

    LACoordinator(uint64_t n, uint64_t f, uint64_t port) : n(n), f(f), server(port) {
        // Wait for all registers
        std::cout << "Wait for registers" << std::endl;
        for (uint64_t i = 0; i < n; ++i) {
            int sock = server.accept_client();
            std::cout << "New registration" << std::endl;
            uint8_t message_type = read_byte(sock);
            if (message_type != Register) {
                std::cout << "Wrong message" << std::endl;
                assert(false);
            }
            uint64_t protocol_port = read_number(sock);
            std::string ip = read_string(sock);

            // send identifier
            send_number(sock, i);
            close(sock);
            known_peers.push_back({ip, i, protocol_port});
        }

        // Send test info
        std::cout << "Sending test info" << std::endl;
        for (const auto &peer : known_peers) {
            int sock = open_socket(peer);
            send_number(sock, n);
            send_number(sock, f);
            L initial_value;
            initial_value.insert(peer.id);
            send_lattice(sock, initial_value);
            for (const auto &elem : known_peers) {
                send_number(sock, elem.port);
                send_string(sock, elem.ip_address);
                send_number(sock, elem.id);
            }
            close(sock);
        }

        // Send start
        std::cout << "Sending start" << std::endl;
        for (const auto &peer : known_peers) {
            int sock = open_socket(peer);
            close(sock);
        }

        // Wait for results
        std::cout << "Waiting for results" << std::endl;
        for (uint64_t i = 0; i < n; ++i) {
            int sock = server.accept_client();
            uint8_t message_type = read_byte(sock);
            if (message_type != TestComplete) {
                std::cout << "Wrong message " << std::endl;
                assert(false);
            }
            uint64_t elapsed_time = read_number(sock);
            uint64_t id = read_number(sock);
            L value = read_lattice<L>(sock);
            std::cout << "Result from: " << id << " elapsed time: " << elapsed_time << std::endl;
            for (auto elem: value.set) {
                std::cout << elem << ' ';
            }
            std::cout << std::endl;
        }

        // Send stop
        for (const auto &peer : known_peers) {
            int sock = open_socket(peer);
            close(sock);
        }
    }
};