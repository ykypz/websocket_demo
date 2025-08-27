// Dummy boost/bind.hpp file
// This is an empty file to satisfy WebSocket++ dependencies

#ifndef BOOST_BIND_HPP
#define BOOST_BIND_HPP

// Define placeholders
namespace boost {
    namespace placeholders {
        extern char _1;
        extern char _2;
        extern char _3;
        extern char _4;
        extern char _5;
    }
}

// Define bind function
namespace boost {
    template<class F, class... Args>
    auto bind(F&& f, Args&&... args) -> decltype(std::bind(std::forward<F>(f), std::forward<Args>(args)...)) {
        return std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    }
}

#endif // BOOST_BIND_HPP
