#ifndef _UTILS_H
#define _UTILS_H

#include <stdexcept>
#include <cstring>
#include <sstream>

// Helper macro for throwing runtime errors caused by an OS syscall with the error value set in
// errno.
//
// ```
// THROW_ERRNO("error here"); // <-- Suppose this is used on line 123 of file src/file.cpp.
// // Will produce an error similar to this (if errno is the error of "Invalid file descriptor")
// // `error here (Invalid file descriptor) at src/file.cpp:123
// ```
#define THROW_ERRNO(msg)                                                                                   \
    do {                                                                                                   \
        std::ostringstream ss;                                                                             \
        ss << msg << " (" << strerror(errno) << ")" << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        throw std::runtime_error(ss.str());                                                                \
    } while (0)

#define STRINGIFY(thing) #thing
#define TOSTRING(thing) STRINGIFY(thing)

#define UNIMPLEMENTED()                                                                                    \
        throw std::runtime_error("reached uninplemented at " __FILE__ ":" TOSTRING(__LINE__));

#define UNREACHABLE()                                                                                      \
        throw std::runtime_error("reached the unreachable at " __FILE__ ":" TOSTRING(__LINE__));

#endif
