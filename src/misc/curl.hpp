#pragma once

#include <string>
#include <tuple>
#include <vector>

#include <curl/curl.h>

namespace curl {

struct InitContext
{
    inline InitContext()
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        InitContext::context_initialized = true;
    }
    inline ~InitContext()
    {
        curl_global_cleanup();
        InitContext::context_initialized = false;
    }

    inline static bool context_initialized = false;
};


struct Curl
{
    using Buffer = std::vector<char>;

    Curl();
    inline ~Curl() { curl_easy_cleanup(_handle); }

    auto get(std::string url) -> std::tuple<CURLcode, Buffer>;

 private:
    CURL* _handle;
};


}  // namespace curl
