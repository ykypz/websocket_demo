// Dummy boost/ref.hpp file
// This is an empty file to satisfy WebSocket++ dependencies

#ifndef BOOST_REF_HPP
#define BOOST_REF_HPP

#include <functional>

namespace boost {
    template<typename T>
    std::reference_wrapper<T> ref(T& t) {
        return std::ref(t);
    }
}

#endif // BOOST_REF_HPP
