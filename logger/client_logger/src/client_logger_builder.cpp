#include <filesystem>
#include <utility>
#include <not_implemented.h>
#include "../include/client_logger_builder.h"

using namespace nlohmann;

logger_builder& client_logger_builder::add_file_stream(
    std::string const &stream_file_path,
    logger::severity severity) &
{
    auto& severity_entry = _output_streams[severity];

    if (severity_entry.first.empty()) {
        severity_entry.second = false;
    }

    auto& streams_list = severity_entry.first;
    streams_list.emplace_front(std::filesystem::weakly_canonical(stream_file_path).string());
    return *this;
}

logger_builder& client_logger_builder::add_console_stream(
    logger::severity severity) &
{
    _output_streams[severity].second = true;
    return *this;
}

logger_builder& client_logger_builder::transform_with_configuration(
    std::string const &configuration_file_path,
    std::string const &configuration_path) &
{
    std::ifstream file_json(configuration_file_path);
    if (!file_json.is_open())
    {
        throw std::ios_base::failure("Can't open file " + configuration_file_path);
    }

    json json_str;
    file_json >> json_str;
    file_json.close();

    auto it = json_str.find(configuration_path);
    if (it == json_str.end() || !it->is_object())
    {
        return *this;
    }

    if (it->contains("format") && it->at("format").is_string())
    {
        _format = it->at("format").get<std::string>();
    }

    parse_severity(logger::severity::trace, (*it)["trace"]);
    parse_severity(logger::severity::debug, (*it)["debug"]);
    parse_severity(logger::severity::information, (*it)["information"]);
    parse_severity(logger::severity::warning, (*it)["warning"]);
    parse_severity(logger::severity::error, (*it)["error"]);
    parse_severity(logger::severity::critical, (*it)["critical"]);

    return *this;
}

logger_builder& client_logger_builder::clear() &
{
    _output_streams.clear();
    _format = "%m";
    return *this;
}

logger *client_logger_builder::build() const
{
    return new client_logger(_output_streams, _format);
}

logger_builder& client_logger_builder::set_format(const std::string &format) &
{
    _format = format;
    return *this;
}

void client_logger_builder::parse_severity(logger::severity sev, nlohmann::json& j)
{
    if (j.is_null() || !j.is_object()) return;

    if (j.contains("console") && j["console"].is_boolean() && j["console"].get<bool>()) {
        _output_streams[sev].second = true;
    }

    if (j.contains("paths") && j["paths"].is_array()) {
        auto& severity_entry = _output_streams[sev];

        for (const auto& path_item : j["paths"]) {
            if (path_item.is_string()) {
                std::string path = path_item.get<std::string>();
                if (!path.empty()) {
                    severity_entry.first.emplace_front(std::filesystem::weakly_canonical(path).string());
                }
            }
        }
    }
}

logger_builder& client_logger_builder::set_destination(const std::string &format) &
{
    throw not_implemented("logger_builder *client_logger_builder::set_destination(const std::string &format)", "invalid call");
}
