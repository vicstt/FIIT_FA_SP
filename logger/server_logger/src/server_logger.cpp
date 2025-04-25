#include <not_implemented.h>
#include <httplib.h>
#include "../include/server_logger.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

server_logger::~server_logger() noexcept
{
    httplib::Params params = {{"pid", std::to_string(inner_getpid())}};
    _client.Get("/destroy", params, {});
}

logger& server_logger::log(
    const std::string &text,
    logger::severity severity) &
{
    const std::string log_entry =
            "[" + current_date_to_string() + " " + current_time_to_string() + "]" +
            "[" + severity_to_string(severity) + "] " + text;

    std::string pid = std::to_string(inner_getpid());
    httplib::Params params = {
            {"pid", pid},
            {"sev", severity_to_string(severity)},
            {"message", log_entry}
    };

    _client.Get("/log", params, {});

    return *this;
}

server_logger::server_logger(const std::string& dest,
                             const std::unordered_map<logger::severity, std::pair<std::string, bool>> &streams)
        : _client(dest)
{
    if (!_client.is_valid()) {
        throw std::runtime_error("Cannot connect to server: " + dest);
    }

    std::string pid = std::to_string(inner_getpid());
    for (const auto& [sev, stream_info] : streams) {
        httplib::Params params = {
                {"pid", pid},
                {"&sev", severity_to_string(sev)},
                {"&path", stream_info.first},
                {"&console", (stream_info.second ? "1" : "0")}
        };
        _client.Get("/init", params, {});
    }
}

int server_logger::inner_getpid()
{
#ifdef _WIN32
    return ::_getpid();
#elif
    return getpid();
#endif
}

//server_logger::server_logger(const server_logger &other)
//{
//    throw not_implemented("server_logger::server_logger(const server_logger &other)", "your code should be here...");
//}
//
//server_logger &server_logger::operator=(const server_logger &other)
//{
//    throw not_implemented("server_logger &server_logger::operator=(const server_logger &other)", "your code should be here...");
//}

server_logger::server_logger(server_logger&& other) noexcept
        : _client(std::move(other._client)) {}

server_logger &server_logger::operator=(server_logger &&other) noexcept
{
    if (this != &other) {
        this->~server_logger();
        new (this) server_logger(std::move(other));
    }
    return *this;
}
