#ifndef __UTILS_TSTRING_H__
#define __UTILS_TSTRING_H__

#include <algorithm>
#include <string>

namespace utils {

    template<size_t N>
    struct tstring {
        char m_data[N];
        
        consteval tstring(const char (&str)[N]) {
            std::copy_n(str, N, m_data);
        }
    };

}

#endif