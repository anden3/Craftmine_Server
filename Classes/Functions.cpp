#include "Functions.h"

#include <chrono>
#include <random>
#include <sstream>

std::vector<std::string> Split(const std::string &s, const char delim) {
    std::vector<std::string> elements;
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delim)) {
        elements.push_back(item);
    }

    return elements;
}

std::string Join(const std::vector<int> &list, const std::string joiner) {
    std::string result = "";

    for (unsigned long i = 0; i < list.size() - 1; ++i) {
        result += std::to_string(list[i]) + joiner;
    }

    return result + std::to_string(list.back());
}

std::string Join(const std::vector<std::string> &list, const std::string joiner) {
    std::string result = "";

    for (unsigned long i = 0; i < list.size() - 1; ++i) {
        result += list[i] + joiner;
    }

    return result + list.back();
}

std::string To_String(unsigned char* data) {
    return std::string(reinterpret_cast<char*>(data));
}

std::string Get_Timestamp() {
    time_t t = std::time(0);
    struct tm* now = std::localtime(&t);

    return Join({
        now->tm_hour, now->tm_min, now->tm_sec
    }, ":");
}

std::string Get_Name(void* data) {
    return *static_cast<std::string*>(data);
}

int Get_Seed() {
    static std::uniform_int_distribution<int> uni(-2147483648, 2147483647);
    static bool Initialized = false;
    static std::mt19937_64 rng;

    if (!Initialized) {
        Initialized = true;

        int64_t timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::seed_seq ss {
            static_cast<uint32_t>(timeSeed & 0xffffffff),
            static_cast<uint32_t>(timeSeed >> 32)
        };

        rng.seed(ss);
    }

    return uni(rng);
}

std::string To_Hex(unsigned int value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}