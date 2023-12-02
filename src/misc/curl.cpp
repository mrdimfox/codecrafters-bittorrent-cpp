#include "misc/curl.hpp"

#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

#include <curl/curl.h>

namespace curl {

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);

Curl::Curl()
{
    assert(InitContext::context_initialized);
    _handle = curl_easy_init();
    assert(_handle);
}


auto Curl::get(std::string url) -> std::tuple<CURLcode, Buffer>
{
    Buffer data;

    curl_easy_setopt(_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(_handle, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(_handle, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(_handle, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(_handle);

    return {res, data};
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto data = static_cast<Curl::Buffer*>(userdata);

    std::span<char> new_bytes{ptr, size * nmemb};

    data->insert(data->end(), new_bytes.begin(), new_bytes.end());

    return new_bytes.size();
}

}  // namespace curl
