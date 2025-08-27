// Dummy boost/function.hpp file
// This is an empty file to satisfy WebSocket++ dependencies

#ifndef BOOST_FUNCTION_HPP
#define BOOST_FUNCTION_HPP

#include <functional>

namespace boost {
    template<typename Signature>
    using function = std::function<Signature>;
}

#endif // BOOST_FUNCTION_HPP
