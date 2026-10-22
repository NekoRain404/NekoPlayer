#pragma once

#include <string_view>

namespace CommandLineArguments {

inline bool hasOptionBeforeSeparator(int argc, char *const argv[], std::string_view option) {
    for (int index = 1; index < argc; ++index) {
        const auto argument = std::string_view(argv[index]);
        if (argument == "--")
            return false;
        if (argument == option)
            return true;
    }
    return false;
}

} // namespace CommandLineArguments
