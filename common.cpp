#include "common.hpp"

bool workSide( const boost::program_options::variables_map &options, std::string type ) {
    const std::string side = type + "-side";
    const std::string front = type + "-front";
    if( options.count(front) ) return options[front].as<bool>();
    return (options.count("front") || !options.count("back"));
}

std::string build_filename(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char sep = '/';
    if (a.back() == sep) return a + b;
    return a + sep + b;
}


