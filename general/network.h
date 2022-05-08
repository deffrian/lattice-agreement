#pragma once

#include <iostream>
#include <arpa/inet.h>
#include <vector>
#include <thread>
#include <unistd.h>
#include <unordered_set>
#include <condition_variable>

#include "general/logger.h"

struct ProcessDescriptor {
    std::string ip_address;
    uint64_t id;
    uint64_t port;
};

uint8_t read_byte(int client_fd) {
    uint8_t byte;

    ssize_t len = read(client_fd, &byte, 1);
    if (len != 1) {
        LOG(ERROR) << "Error reading byte" << errno;
        throw std::runtime_error("Error reading byte: " + std::to_string(len));
    }
    return byte;
}

uint64_t read_number(int client_fd) {
    uint64_t number;
    size_t bytes_read = 0;

    while (bytes_read != 64 / 8) {
        ssize_t len = read(client_fd, (char*)(&number) + bytes_read, 64 / 8 - bytes_read);
        if (len <= 0) {
            LOG(ERROR) << "Error reading number" << errno;
            throw std::runtime_error("Error reading number: " + std::to_string(len));
        }
        bytes_read += len;
    }
    return number;
}

std::string read_string(int client_fd) {
    uint64_t s_len = read_number(client_fd);
    std::string s(s_len, ' ');
    size_t bytes_read = 0;

    while (bytes_read != s_len) {
        ssize_t len = read(client_fd, s.data() + bytes_read, s_len - bytes_read);
        if (len <= 0) {
            LOG(ERROR) << "Error reading string" << errno;
            throw std::runtime_error("Error reading string: " + std::to_string(len));
        }
        bytes_read += len;
    }
    return s;
}

template<typename L>
L read_lattice(int client_fd) {
    L res;
    uint64_t size = read_number(client_fd);
    std::vector<uint64_t> data(size);
    size_t bytes_read = 0;

    while (bytes_read != size * 8) {
        ssize_t len = read(client_fd, (char*)(data.data()) + bytes_read, 8 * size - bytes_read);
        if (len <= 0) {
            LOG(ERROR) << "Error reading lattice" << errno;
            throw std::runtime_error("Error reading number: " + std::to_string(len));
        }
        bytes_read += len;
    }
    for (uint64_t i = 0; i < size; ++i) {
        res.insert(data[i]);
    }
    return res;
}

template<typename L>
std::vector<L> read_lattice_vector(int client_fd) {
    uint64_t v_size = read_number(client_fd);
    std::vector<L> v(v_size);
    for (uint64_t i = 0; i < v_size; ++i) {
        v[i] = read_lattice<L>(client_fd);
    }
    return v;
}

int open_socket(const ProcessDescriptor &descriptor) {
    LOG(INFO) << "Open connection to " << descriptor.id << " port: " << descriptor.port << " ip: " << descriptor.ip_address;
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG(ERROR) << "Socket creation error" << errno;
        throw std::runtime_error("Socket creation error");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(descriptor.port);

    if (inet_pton(AF_INET, descriptor.ip_address.data(), &serv_addr.sin_addr) <= 0) {
        LOG(ERROR) << "Invalid address";
        throw std::runtime_error("Invalid address");
    }

    if (connect(sock, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) {
        LOG(ERROR) << "Connection failed: " << descriptor.ip_address << descriptor.port << descriptor.id << " error:" << errno;
        throw std::runtime_error("Connection failed");
    }
//    struct timeval tv;
//
//    tv.tv_sec = 3000000;
//
//    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval))) {
//        throw std::runtime_error("Client timeout setup failed");
//    }
    return sock;
}


void send_byte(int sock, uint8_t byte) {
    ssize_t len = send(sock, &byte, 1, 0);
    if (len != 1) {
        LOG(ERROR) << "Error sending byte:" << errno;
        throw std::runtime_error("Error sending byte: " + std::to_string(len));
    }
}

void send_number(int sock, uint64_t num) {
    ssize_t len = send(sock, &num, 64 / 8, 0);
    if (len != 64 / 8) {
        LOG(ERROR) << "Error sending number:" << errno;
        throw std::runtime_error("Error sending number: " + std::to_string(len));
    }
}

void send_string(int sock, const std::string &s) {
    send_number(sock, s.length());
    ssize_t len = send(sock, s.data(), s.length(), 0);
    if (len != s.length()) {
        LOG(ERROR) << "error sending string:" << errno;
        throw std::runtime_error("Error sending string: " + std::to_string(len));
    }
}

template<typename L>
void send_lattice(int sock, const L &lattice) {
    send_number(sock, lattice.set.size());
    for (uint64_t elem : lattice.set) {
        send_number(sock, elem);
    }
}

template<typename L>
void send_lattice_vector(int sock, const std::vector<L> &v) {
    send_number(sock, v.size());
    for (const auto &elem: v) {
        send_lattice(sock, elem);
    }
}

template<typename L>
void send_recVal(int sock, const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal) {
    send_number(sock, recVal.size());
    for (auto elem: recVal) {
        send_lattice_vector(sock, elem.first);
        send_number(sock, elem.second);
    }
}

struct TcpServer {
    int server_fd;

//    std::vector<int> all_connections;
//    std::unordered_set<int> used_fds;

    std::mutex connections_mt;
    std::condition_variable connection_cv;
    const size_t MAX_INCOMING_CONNECTIONS = 10;
    uint64_t connections_number = 0;

    struct sockaddr_in address;
    int addrlen;

    explicit TcpServer(uint64_t port) {
        int opt = 1;
        addrlen = sizeof(address);

        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            throw std::runtime_error("Server creation failed");
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            throw std::runtime_error("Server creation failed");
        }

//        struct timeval tv;
//
//        tv.tv_sec = 3000000;
//
//        if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval))) {
//            LOG(ERROR) << errno;
//            throw std::runtime_error("Server timeout setup error");
//        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
            throw std::runtime_error("Server creation failed");
        }
        if (listen(server_fd, 1000) < 0) {
            throw std::runtime_error("Server creation failed");
        }
//        all_connections[0] = server_fd;
    }

    int accept_client() {
        {
            std::unique_lock lk{connections_mt};
            while (connections_number >= MAX_INCOMING_CONNECTIONS) { connection_cv.wait(lk); }
            connections_number++;
        }
        struct sockaddr_in client_addr;
        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen);
        if (client_fd <= 0) {
            LOG(ERROR) << "Cant accept client" << errno;
            throw std::runtime_error("Cant accept client");
        }
//        LOG(ERROR) << "NEW ACCEPTED CLIENT" << client_fd;

//        struct timeval tv;
//
//        tv.tv_sec = 3000000;
//
//        if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval))) {
//            throw std::runtime_error("Client timeout setup failed");
//        }
//        all_connections.push_back(client_fd);
        return client_fd;
    }

    std::vector<int> select_clients() {
//        while (true) {
//            fd_set read_fd_set;
//            FD_ZERO(&read_fd_set);
//            {
//                std::lock_guard<std::mutex> lg{connections_mt};
//
////                LOG tmp(INFO);
////                tmp << "Used connections" << all_connections.size() << used_fds.size();
//                for (size_t i = 0; i < all_connections.size(); ++i) {
//                    if (used_fds.count(all_connections[i]) == 0) {
//                        FD_SET(all_connections[i], &read_fd_set);
//                    }
//                }
//            }
//            struct timeval timeout;
//            timeout.tv_sec = 0;
//            timeout.tv_usec = 50000;
////        LOG(INFO) << "SELECTING";
//            int select_val = select(FD_SETSIZE, &read_fd_set, nullptr, nullptr, &timeout);
//
//            if (select_val != 0) {
//                std::lock_guard<std::mutex> lg{connections_mt};
//
//                if (select_val < 0) {
//                    LOG(ERROR) << "Select error" << select_val;
//                    throw std::runtime_error("Select error");
//                }
//
//                std::vector<int> ans;
//                for (size_t i = 0; i < all_connections.size(); i++) {
//                    if ((FD_ISSET(all_connections[i], &read_fd_set)) && used_fds.count(all_connections[i]) == 0) {
////                        LOG(INFO) << "Opened" << all_connections[i];
//                        used_fds.insert(all_connections[i]);
//                        ans.push_back(all_connections[i]);
//                    }
//                }
//                if (!ans.empty()) {
//                    return ans;
//                }
//            }
//        }
        exit(EXIT_FAILURE);
    }

    void close_socket(int client_fd) {
//        LOG(INFO) << "Trying to close" << client_fd;
//        std::lock_guard<std::mutex> lg{connections_mt};
//        LOG(INFO) << "Closed" << client_fd;
//        used_fds.erase(client_fd);
        std::lock_guard<std::mutex> lg{connections_mt};
//        LOG(ERROR) << "CLOSING SOCKET" << client_fd;
        connection_cv.notify_one();
        connections_number--;
        int res = close(client_fd);
//        if (res == -1) {
//            LOG(ERROR) << "Error closing socket" << errno;
//        }
    }

    int accept_client(std::string &client_ip) {
        struct sockaddr_in client_addr;
        client_ip.resize(INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip.data(), INET_ADDRSTRLEN);
        return accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen);
    }

};
