#include <not_implemented.h>
#include "../include/server_logger_builder.h"
#include <fstream>
#include <utility>
using namespace nlohmann;

logger_builder& server_logger_builder::add_file_stream(
    std::string const &stream_file_path,
    logger::severity severity) &{
    auto severity_entry = _output_streams.find(severity);

    if (severity_entry == _output_streams.end()) {
        severity_entry = _output_streams.emplace(severity, std::make_pair(stream_file_path,false)).first;                       // Получаем итератор на вставленный элемент
    }

    severity_entry->second.first = stream_file_path;

    return *this;
}

logger_builder& server_logger_builder::add_console_stream(
    logger::severity severity) &
{
    _output_streams[severity].second = true;
    return *this;
}

logger_builder& server_logger_builder::transform_with_configuration(
    std::string const &configuration_file_path,
    std::string const &configuration_path) &
{
    std::ifstream config_file(configuration_file_path);
    if (!config_file) {
        return *this;
    }

    json config = json::parse(config_file, nullptr, false);
    if (config.is_discarded()) {
        return *this;
    }

    if (!config.contains(configuration_path)) {
        return *this;
    }
    json& section = config[configuration_path];

    if (section.contains("format")) {
        set_format(section["format"]);
    }

    if (section.contains("streams") && section["streams"].is_array()) {
        for (auto& stream : section["streams"]) {
            if (!stream.is_object()) continue;

            if (!stream.contains("type") || !stream["type"].is_string()) continue;
            std::string type = stream["type"];

            if (!stream.contains("severities") || !stream["severities"].is_array()) continue;

            if (type == "file" && stream.contains("path") && stream["path"].is_string()) {
                std::string path = stream["path"];
                for (auto& severity : stream["severities"]) {
                    if (severity.is_string()) {
                        add_file_stream(path, string_to_severity(severity.get<std::string>()));
                    }
                }
            }
            else if (type == "console") {
                for (auto& severity : stream["severities"]) {
                    if (severity.is_string()) {
                        add_console_stream(string_to_severity(severity.get<std::string>()));
                    }
                }
            }
        }
    }

    return *this;
}

logger_builder& server_logger_builder::clear() &
{
    _output_streams.clear();
    _destination = "http://127.0.0.1:9200";

    return *this;
}

logger *server_logger_builder::build() const
{
    return new server_logger(_destination, _output_streams);
}

logger_builder& server_logger_builder::set_destination(const std::string& dest) &
{
    _destination = dest;
    return *this;
}

logger_builder& server_logger_builder::set_format(const std::string &format) &
{
    throw not_implemented("logger_builder& server_logger_builder::set_format(const std::string &) &", "your code should be here...");
}
