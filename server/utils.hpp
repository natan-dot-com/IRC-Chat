#ifndef _UTILS_H
#define _UTILS_H

#include <stdexcept>
#include <cstring>
#include <sstream>

#define THROW_ERRNO(msg) \
    do { \
        std::ostringstream ss; \
        ss << msg << "(" << strerror(errno) << ")" << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        throw std::runtime_error(ss.str()); \
    } while (0)

#endif
