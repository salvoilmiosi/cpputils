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

        consteval operator std::string_view() const {
            return std::string_view{m_data, m_data+N};
        }
    };

}

#endif