#include "misc/curl.hpp"

#include <cassert>
#include <chrono>
#include <cstddef>
#include <span>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <fmt/core.h>

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

auto Curl::tcp_transfer(std::string ip, std::string port, const Buffer& message)
  -> std::tuple<CURLcode, Buffer>
{
    Buffer data;

    curl_easy_setopt(
      _handle, CURLOPT_URL, fmt::format("{}:{}", ip, port).c_str()
    );

    curl_easy_setopt(_handle, CURLOPT_CONNECT_ONLY, 1L);

    if (auto res = curl_easy_perform(_handle); res != CURLcode::CURLE_OK) {
        return {res, data};
    }

    curl_socket_t socket;
    auto res = curl_easy_getinfo(_handle, CURLINFO_ACTIVESOCKET, &socket);

    // TODO: send in cycle
    size_t sent = 0;
    res = curl_easy_send(_handle, message.data(), message.size(), &sent);
    if (res != CURLcode::CURLE_OK) {
        return {res, data};
    }

    size_t received = 0;
    data.resize(256);

    // TODO: select(2)?
    auto recv_res = CURLcode::CURLE_AGAIN;
    while (recv_res == CURLcode::CURLE_AGAIN) {
        recv_res = curl_easy_recv(_handle, data.data(), data.size(), &received);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return {recv_res, data};
}


size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto data = static_cast<Curl::Buffer*>(userdata);

    std::span<Curl::Buffer::value_type> new_bytes{
      reinterpret_cast<Curl::Buffer::value_type*>(ptr), size * nmemb
    };

    data->insert(data->end(), new_bytes.begin(), new_bytes.end());

    return new_bytes.size();
}

}  // namespace curl
