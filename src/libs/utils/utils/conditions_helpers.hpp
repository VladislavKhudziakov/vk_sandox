#pragma once

#include <stdexcept>

#ifndef NDEBUG
    #define CHECK(expr)                                                                                                         \
        if (!expr) {                                                                                                            \
            throw std::runtime_error(#expr " failed in " + std::string(__FILE__) " at " + std::to_string(__LINE__) + " line."); \
        }
#else
    #define CHECK(expr)                                 \
        if (!expr) {                                    \
            throw std::runtime_error(#expr " failed."); \
        }
#endif

#ifndef NDEBUG
    #define CHECK_MSG(expr, msg)                                                                                                                      \
        if (!(expr)) {                                                                                                                                \
            throw std::runtime_error(#expr " failed in " + std::string(__FILE__) + " at " + std::to_string(__LINE__) + " line. " + std::string(msg)); \
        }
#else
    #define CHECK_MSG(expr, msg)           \
        if (!(expr)) {                     \
            throw std::runtime_error(msg); \
        }
#endif