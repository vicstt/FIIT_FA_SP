#include <string>
#include <sstream>
#include <algorithm>
#include <utility>
#include <unordered_set>
#include "../include/client_logger.h"

std::unordered_map<std::string, std::pair<size_t, std::ofstream>> client_logger::refcounted_stream::_global_streams;


logger& client_logger::log(
    const std::string &text,
    logger::severity severity) &
{
    std::string output = make_format(text, severity);
    auto it = _output_streams.find(severity);
    if (it == _output_streams.end()) return *this;

    if (it->second.second) {
        std::cout << output << std::endl;
    }

    std::unordered_set<std::string> written_files;
    for (const auto& stream : it->second.first) {
        if (stream._stream.second != nullptr && written_files.insert(stream._stream.first).second) {
            *(stream._stream.second) << output << std::endl;
        }
    }
    return *this;
}

std::string client_logger::make_format(const std::string &message, severity sev) const
{
    std::stringstream res_format;
    for (auto iter = _format.begin(), end = _format.end(); iter != end; ++iter) {
        if (*iter == '%' && iter + 1 != end) {
            flag fl = char_to_flag(*(iter + 1));
            switch (fl) {
                case flag::DATE:
                    res_format << current_date_to_string();
                    break;
                case flag::TIME:
                    res_format << current_time_to_string();
                    break;
                case flag::SEVERITY:
                    res_format << severity_to_string(sev);
                    break;
                case flag::MESSAGE:
                    res_format << message;
                    break;
                default:
                    res_format << "%" << *(iter + 1);
                    break;
            }
            ++iter;
        } else {
            res_format << *iter;
        }
    }
    return res_format.str();
}

client_logger::client_logger(
        const std::unordered_map<logger::severity, std::pair<std::forward_list<refcounted_stream>, bool>> &streams,
        std::string format) : _output_streams(streams), _format(std::move(format)) {}

client_logger::flag client_logger::char_to_flag(char c) noexcept
{
    switch (c) {
        case 'd': return flag::DATE;
        case 't': return flag::TIME;
        case 's': return flag::SEVERITY;
        case 'm': return flag::MESSAGE;
        default: return flag::NO_FLAG;
    }
}

client_logger::client_logger(const client_logger &other)
        : _output_streams(other._output_streams),
          _format(other._format) {}

//client_logger &client_logger::operator=(const client_logger &other)
//{
//    throw not_implemented("client_logger::flag client_logger::char_to_flag(char) noexcept", "your code should be here...");
//}

client_logger::client_logger(client_logger &&other) noexcept :
        _output_streams(std::move(other._output_streams)),
        _format(std::move(other._format))
{}

//client_logger &client_logger::operator=(client_logger &&other) noexcept
//{
//    throw not_implemented("client_logger &client_logger::operator=(client_logger &&other) noexcept", "your code should be here...");
//}

client_logger::~client_logger() noexcept
{
    for (auto& level : _output_streams) {
        for (auto& stream : level.second.first) {
            if (stream._stream.second != nullptr) {
                std::string path = stream._stream.first;
                auto it = refcounted_stream::_global_streams.find(path);
                if (it != refcounted_stream::_global_streams.end()) {
                    it->second.first--;
                    if (it->second.first == 0) {
                        it->second.second.close();
                        refcounted_stream::_global_streams.erase(it);
                    }
                }
            }
        }
    }
}

client_logger::refcounted_stream::refcounted_stream(const std::string &path)
{
    auto it = _global_streams.find(path);

    if (it == _global_streams.end()) {
        std::ofstream new_stream(path);
        if (!new_stream.is_open()) {
            throw std::ios_base::failure("Can't open file " + path);
        }

        auto inserted = _global_streams.insert({path, {1, std::move(new_stream)}});
        _stream = std::make_pair(path, &inserted.first->second.second);
    } else {
        it->second.first++;
        _stream = std::make_pair(path, &it->second.second);
    }
}

client_logger::refcounted_stream::refcounted_stream(const client_logger::refcounted_stream &oth)
        : _stream(oth._stream) {
    if (_stream.second && !_global_streams.empty())
    {
        auto it = _global_streams.find(_stream.first);
        if (it != _global_streams.end())
            ++it->second.first;
    }
}

client_logger::refcounted_stream &
client_logger::refcounted_stream::operator=(const client_logger::refcounted_stream &oth)
{
    if (this != &oth) {
        if (_stream.second) {
            auto old_it = _global_streams.find(_stream.first);
            if (old_it != _global_streams.end() && --old_it->second.first == 0) {
                old_it->second.second.close();
                _global_streams.erase(old_it);
            }
        }

        _stream = oth._stream;
        if (_stream.second) {
            auto new_it = _global_streams.find(_stream.first);
            if (new_it != _global_streams.end()) {
                ++new_it->second.first;
            }
        }
    }
    return *this;
}

client_logger::refcounted_stream::refcounted_stream(client_logger::refcounted_stream &&oth) noexcept
        : _stream(std::move(oth._stream))
{
    oth._stream = { "", nullptr };
}

client_logger::refcounted_stream &client_logger::refcounted_stream::operator=(client_logger::refcounted_stream &&oth) noexcept {
    if (this != &oth) {
        if (_stream.second) {
            auto it = _global_streams.find(_stream.first);
            if (it != _global_streams.end() && --it->second.first == 0) {
                it->second.second.close();
                _global_streams.erase(it);
            }
        }

        _stream = std::move(oth._stream);
        oth._stream = { "", nullptr };
    }
    return *this;
}

client_logger::refcounted_stream::~refcounted_stream()
{
    if (_stream.second && !_stream.first.empty()) {
        auto it = _global_streams.find(_stream.first);
        if (it != _global_streams.end() && it->second.first > 0) {
            if (--it->second.first == 0) {
                if (it->second.second.is_open()) {
                    it->second.second.close();
                }
                _global_streams.erase(it);
            }
        }
    }
}
