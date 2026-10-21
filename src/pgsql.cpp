#include "pgsql.h"

#include <libpq-fe.h>
#include <memory>
#include <stdexcept>

namespace sc {
    namespace pg {
        using result = std::unique_ptr<PGresult, decltype(&PQclear)>;

        class connection {
        public:
            explicit connection(const std::string &conninfo) : conn_{PQconnectdb(conninfo.c_str()), &PQfinish} {
                if (PQstatus(conn_.get()) != CONNECTION_OK) {
                    throw std::runtime_error{PQerrorMessage(conn_.get())};
                }
            }

            result exec(const std::string &query, const std::vector<std::string> &params) const {
                if (params.empty()) return {PQexec(conn_.get(), query.c_str()), &PQclear};
                std::vector<const char *> values;
                values.reserve(params.size());
                for (const auto &param: params) values.push_back(param.c_str());
                return {PQexecParams(conn_.get(), query.c_str(), static_cast<int>(values.size()), nullptr, values.data(), nullptr, nullptr, 0), &PQclear};
            }

            std::string error_message() const { return PQerrorMessage(conn_.get()); }

        private:
            std::unique_ptr<PGconn, decltype(&PQfinish)> conn_;
        };
    }

    namespace {
        // A libpq connection string value, quoted so spaces, quotes and backslashes survive.
        std::string conninfo_value(const std::string &value) {
            std::string quoted = "'";
            for (const char c: value) {
                if (c == '\'' || c == '\\') quoted += '\\';
                quoted += c;
            }
            return quoted + '\'';
        }
    }

    postgres::postgres(const ip_endpoints &servers, const std::string &db, const std::string &user,
                       const std::string &passwd) {
        std::string hosts;
        std::string ports;
        bool any_port = false;
        for (const auto &server: servers) {
            if (!hosts.empty()) {
                hosts += ',';
                ports += ',';
            }
            hosts += server.host;
            if (server.port) {
                ports += std::to_string(server.port);
                any_port = true;
            }
        }

        // An empty value is left out rather than passed as '', which would stop libpq falling
        // back to its environment variables (PGHOST, PGPORT, PGDATABASE, PGUSER, PGPASSWORD).
        std::string conninfo;
        const auto add = [&conninfo](const char *keyword, const std::string &value) {
            if (value.empty()) return;
            if (!conninfo.empty()) conninfo += ' ';
            conninfo += keyword;
            conninfo += '=';
            conninfo += conninfo_value(value);
        };
        add("host", hosts);
        if (any_port) add("port", ports);
        add("dbname", db);
        add("user", user);
        add("password", passwd);
        impl = std::make_unique<pg::connection>(conninfo);
    }

    postgres::~postgres() = default;

    postgres::postgres(postgres &&other) noexcept = default;

    postgres &postgres::operator=(postgres &&other) noexcept = default;

    std::vector<std::map<std::string, std::string> > postgres::exec(const std::string &query, const std::vector<std::string> &parameters) const {
        if (!impl) throw std::logic_error("sc::postgres: exec() on a connection that was moved from");
        const auto res = impl->exec(query, parameters);
        if (!res) throw std::runtime_error{"No result"};
        const auto status = PQresultStatus(res.get());
        if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) throw std::runtime_error{impl->error_message()};

        const int rows = PQntuples(res.get());
        const int fields = PQnfields(res.get());
        std::vector<std::string> names;
        names.reserve(fields);
        for (int j = 0; j < fields; ++j) names.emplace_back(PQfname(res.get(), j));

        std::vector<std::map<std::string, std::string> > result(rows);
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < fields; ++j) {
                result[i][names[j]] = PQgetvalue(res.get(), i, j);
            }
        }
        return result;
    }
} // sc
