#pragma once

#include "magic_enum.hpp"
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <curl/curl.h>
#include <fmt/core.h>

namespace curl {

struct Url
{

    inline Url() : _handle(curl_url()) {}

    inline ~Url() { curl_url_cleanup(_handle); }

    inline Url& base(std::string_view base)
    {
        _set(CURLUPART_URL, base, 0);
        return *this;
    }

    template<typename ParamT>
    inline Url& query(std::string_view name, ParamT param)
    {
        _set(
          CURLUPart::CURLUPART_QUERY,
          fmt::format("{0}={1}", name, param), CURLU_APPENDQUERY
        );

        return *this;
    }

    inline std::string to_string()
    {
        return _get(CURLUPart::CURLUPART_URL, CURLU_NO_DEFAULT_PORT);
    }

 private:
    inline auto _set(CURLUPart what, std::string_view part, unsigned int flags)
      -> void
    {
        auto code = curl_url_set(_handle, what, part.data(), flags);

        if (code != CURLUcode::CURLUE_OK) {
            throw std::invalid_argument(fmt::format(
              "Can not set URL part {0} with provided argument {1}. Error "
              "code: {2}",
              magic_enum::enum_name(what), part, magic_enum::enum_name(code)
            ));
        }
    }

    inline auto _get(CURLUPart what, unsigned int flags) -> std::string
    {
        char* part_c_str;

        auto code = curl_url_get(_handle, what, &part_c_str, flags);

        if (code != CURLUcode::CURLUE_OK) {
            curl_free(part_c_str);

            throw std::invalid_argument(fmt::format(
              "Can not get URL part {0}. Error code: {1}",
              magic_enum::enum_name(what), magic_enum::enum_name(code)
            ));
        }

        std::string part_str{part_c_str};
        curl_free(part_c_str);

        return part_str;
    }

    Curl_URL* _handle;
};

template<>
inline Url& Url::query<std::vector<std::uint8_t>>(
  std::string_view name, std::vector<std::uint8_t> param
)
{
    std::string bytes_str{param.begin(), param.end()};

    _set(
      CURLUPART_QUERY, fmt::format("{0}={1}", name, bytes_str).data(),
      CURLU_APPENDQUERY | CURLU_URLENCODE
    );

    return *this;
}

}  // namespace curl
