#pragma once

#include <sstream>
#include <iostream>
#include <vector>

#include "lattice.h"

/**
 * Logging level
 */
enum LogLevel {
    ERROR,
    INFO,
};
/**
 * Used to log messages. Usage: LOG(INFO) << "Logging message"
 */
struct LOG {
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

    template<typename L, typename R>
    LOG & operator<<(const std::pair<L, R> &message) {
        ss << '(';
        *this << message.first << message.second;
        ss << ")";
        return *this;
    }

private:
    std::stringstream ss;
    LogLevel level;

};