#pragma once

#include <utility>
#include <vector>

#include "general/network.h"

enum CoordinatorMessage : uint8_t {
    Register = 0,
    TestComplete = 1,
    Start = 2,
    Stop = 3,
    TestInfo = 4
};

template<typename L>
struct LACoordinatorClient {

    TcpServer server;
    ProcessDescriptor coordinator_descriptor;

    uint64_t my_id = -1;

    LACoordinatorClient(uint64_t port, ProcessDescriptor coordinator_descriptor) : server(port),
                                                                                   coordinator_descriptor(std::move(
                                                                                           coordinator_descriptor)) {}

    uint64_t send_register(uint64_t protocol_port, uint64_t coordinator_client_port, const std::string &ip) {
        int sock = open_socket(coordinator_descriptor);
        send_byte(sock, Register);
        send_number(sock, protocol_port);
        send_number(sock, coordinator_client_port);
        send_string(sock, ip);
        my_id = read_number(sock);
        close(sock);
        return my_id;
    }

    void wait_for_test_info(uint64_t &n, uint64_t &f, L &initial_value, std::vector<ProcessDescriptor> &peers) {
        int sock = server.accept_client();
        uint8_t message_type = read_byte(sock);
        if (message_type != TestInfo) {
            LOG(ERROR) << "Wrong message in wait for test info";
            throw std::runtime_error("Wrong message in wait for test info");
        }
        n = read_number(sock);
        f = read_number(sock);
        initial_value = read_lattice<L>(sock);
        for (uint64_t i = 0; i < n; ++i) {
            uint64_t port = read_number(sock);
            std::string ip = read_string(sock);
            uint64_t id = read_number(sock);
//            if (id != my_id) {
                peers.push_back({ip, id, port});
//            }
        }
        server.close_socket(sock);
    }

    void wait_for_start() {
        int sock = server.accept_client();
        uint8_t byte = read_byte(sock);
        if (byte != Start) {
            LOG(ERROR) << "Wrong message in wait for start";
            throw std::runtime_error("Wrong message in wait for start");
        }
        server.close_socket(sock);
    }

    void wait_for_stop() {
        int sock = server.accept_client();
        uint8_t byte = read_byte(sock);
        if (byte != Stop) {
            LOG(ERROR) << "Wrong message in wait for stop";
            throw std::runtime_error("Wrong message in wait for stop");
        }
        server.close_socket(sock);
    }

    void send_test_complete(uint64_t elapsed_time, const L &received_value) {
        int sock = open_socket(coordinator_descriptor);
        send_byte(sock, TestComplete);
        send_number(sock, elapsed_time);
        send_number(sock, my_id);
        send_lattice(sock, received_value);
        close(sock);
    }
};

template<typename L>
struct LACoordinator {
    uint64_t n;
    uint64_t f;

    TcpServer server;

    std::vector<ProcessDescriptor> known_peers;
    std::vector<ProcessDescriptor> coordinator_clients;

    LACoordinator(uint64_t n, uint64_t f, uint64_t port) : n(n), f(f), server(port) {
        // Wait for all registers
        LOG(INFO) << "Wait for registers";
        for (uint64_t i = 0; i < n; ++i) {
            int sock = server.accept_client();
            LOG(INFO) << "New registration";
            uint8_t message_type = read_byte(sock);
            if (message_type != Register) {
                LOG(ERROR) << "Wrong message";
                throw std::runtime_error("Wrong message");
            }
            uint64_t protocol_port = read_number(sock);
            uint64_t coordinator_client_port = read_number(sock);
            std::string ip = read_string(sock);

            // send identifier
            send_number(sock, i);
            close(sock);
            known_peers.push_back({ip, i, protocol_port});
            coordinator_clients.push_back({ip, i, coordinator_client_port});
            LOG(INFO) << "ip: " << ip << " id: " << i << " port: " << protocol_port << " coord_client_port: "
                      << coordinator_client_port;
            server.close_socket(sock);
        }

        // Send test info
        LOG(INFO) << "Sending test info";
        for (const auto &peer : coordinator_clients) {
            int sock = open_socket(peer);
            send_byte(sock, TestInfo);
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
        LOG(INFO) << "Sending start";
        for (const auto &peer : coordinator_clients) {
            int sock = open_socket(peer);
            send_byte(sock, Start);
            close(sock);
        }

        // Wait for results
        LOG(INFO) << "Waiting for results";
        uint64_t total_time = 0;
        std::vector<std::pair<uint64_t, L>> results;
        std::cout << std::fixed;
        for (uint64_t i = 0; i < n; ++i) {
            try {
                int sock = server.accept_client();
                uint8_t message_type = read_byte(sock);
                if (message_type != TestComplete) {
                    LOG(ERROR) << "Wrong message in wait for results" << (int) message_type;
                    throw std::runtime_error("Wrong message in wait for results " + std::to_string((int) message_type));
                }
                uint64_t elapsed_time = read_number(sock);
                total_time += elapsed_time;
                uint64_t id = read_number(sock);
                L value = read_lattice<L>(sock);
                results.push_back({id, value});
                LOG(INFO) << "Result from: " << id << " elapsed time: " << elapsed_time;
                for (auto elem: value.set) {
                    std::cout << elem << ' ';
                }
                std::cout << std::endl;
                LOG(INFO) << "Current average time:" << (double) total_time / (double) n;
                LOG(INFO) << "Done" << i + 1 << "/" << n;
                server.close_socket(sock);
            } catch (std::runtime_error &e) {
                LOG(ERROR) << "* Exception while waiting for results" << e.what();
            }
        }
        LOG(INFO) << "Total average time:" << (double) total_time / (double) n;

        // Send stop
        for (const auto &peer : coordinator_clients) {
            try {
                int sock = open_socket(peer);
                send_byte(sock, Stop);
                close(sock);
            } catch (std::runtime_error &e) {
                LOG(ERROR) << "* Exception" << "Cant stop process" << peer.id << e.what();
            }
        }

        LOG(INFO) << "Verifying";
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                bool fTos = results[i].second <= results[j].second;
                bool sTof = results[j].second <= results[i].second;
                if (!fTos && !sTof) {
                    LOG(ERROR) << "Invalid results" << results[i].first << results[j].first;
                }
            }
        }
    }
};