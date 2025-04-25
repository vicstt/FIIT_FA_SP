#include "server.h"
#include <logger_builder.h>
#include <fstream>
#include <iostream>


server::server(uint16_t port)
{
    CROW_ROUTE(app, "/<string>")([this](const crow::request& req, std::string type) {
        if (type == "init") {
            auto pid = req.url_params.get("pid");
            auto sev_str = req.url_params.get("sev");
            auto path = req.url_params.get("path");
            auto console = req.url_params.get("console");

            if (!pid || !sev_str || !console) return crow::response(400);

            logger::severity sev = logger_builder::string_to_severity(sev_str);

            std::cout << "INIT PID:" << pid << " SEV:" << sev_str
                      << " PATH:" << (path ? path : "")
                      << " CONSOLE:" << console << "\n";

            _streams[std::stoi(pid)][sev] = {path ? path : "", std::string(console) == "1"};
            return crow::response(200);
        }
        else if (type == "destroy") {
            auto pid = req.url_params.get("pid");
            if (!pid) return crow::response(400);

            std::cout << "DESTROY PID:" << pid << std::endl;
            _streams.erase(std::stoi(pid));
            return crow::response(200);
        }
        else if (type == "log") {
            auto pid = req.url_params.get("pid");
            auto sev_str = req.url_params.get("sev");
            auto msg = req.url_params.get("message");

            if (!pid || !sev_str || !msg) {
                return crow::response(400);
            }

            logger::severity sev = logger_builder::string_to_severity(sev_str);

            std::cout << "LOG PID:" << pid << " SEV:" << sev_str << " MSG:" << msg << std::endl;

            auto& target = _streams[std::stoi(pid)][sev];
            if (!target.first.empty()) {
                std::ofstream file(target.first, std::ios::app);
                file << msg << "\n";
            }
            if (target.second) std::cout << msg << std::endl;
            return crow::response(200);
        }

        return crow::response(404);
    });

    app.port(port).loglevel(crow::LogLevel::Warning).multithreaded().run();
}
