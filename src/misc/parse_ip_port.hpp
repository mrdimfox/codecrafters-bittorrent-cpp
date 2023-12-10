#pragma once

#include <regex>
#include <string>
#include <tuple>

#include <fmt/core.h>

namespace utils {

inline auto parse_ip_port(std::string ip_port_str)
  -> std::tuple<std::string, std::string>
{
    std::regex port_ip_regex(  //
      R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}):(\d{1,5}))"
    );
    std::smatch match;

    if (not std::regex_match(ip_port_str, match, port_ip_regex)) {
        throw std::runtime_error(fmt::format(
          "Peer ip and port must be in format \"<d.d.d.d>:<d>\" (d means "
          "digit). Found: {0}",
          ip_port_str
        ));
    }

    auto ip = match[1].str();
    auto port = match[2].str();

    return {ip, port};
}

}  // namespace utils
