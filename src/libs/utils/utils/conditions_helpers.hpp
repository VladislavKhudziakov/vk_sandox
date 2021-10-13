#pragma once

#include <stdexcept>
#include <cassert>

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)

#ifndef NDEBUG
    #define CHECK(expr)                                                                                                           \
        if (!(expr)) {                                                                                                            \
            throw std::runtime_error(#expr " failed in " + std::string(__FILE__) + " at " + std::to_string(__LINE__) + " line."); \
        }
#else
    #define CHECK(expr)                                 \
        if (!(expr)) {                                  \
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

#ifndef NDEBUG
    #define ASSERT(expr) assert(expr)
#else
    #define ASSERT(expr) CHECK(expr)
#endif

#undef STRINGIZE
#undef STRINGIZE2
#undef LINE_STRING