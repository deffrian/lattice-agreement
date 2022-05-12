#pragma once

#include <sstream>
#include <iostream>
#include "lattice.h"

enum LogLevel {
    ERROR,
    INFO,
};

struct LOG {

    std::stringstream ss;
    LogLevel level;

    explicit LOG(LogLevel level) : level(level) {}

    ~LOG() {
        ss << '\n';
        if (level == ERROR) {
            std::cerr << ss.str();
            std::cerr.flush();
        } else {
            std::cout << ss.str();
            std::cout.flush();
        }
    }

    template<typename T>
    LOG & operator<<(const T& message) {
        ss << message << ' ';
        return *this;
    }

    template<typename T>
    LOG & operator<<(const std::vector<T> &message) {
        ss << '{';
        for (auto elem : message) {
            *this << elem << ',';
        }
        ss << "} ";
        return *this;
    }

    template<>
    LOG & operator<<(const LatticeSet &message) {
        ss << '{';
        for (auto elem : message.set) {
            ss << elem << ',';
        }
        ss << "} ";
        return *this;
    }

};