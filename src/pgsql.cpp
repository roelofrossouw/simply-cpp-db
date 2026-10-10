#include "pgsql.h"

#include <libpq-fe.h>
#include <stdexcept>
#include <utility>

namespace sc {
    namespace pg {
        class connection {
            PGconn *conn_ = nullptr;

        public:
            explicit connection(const std::string &conninfo) {
                conn_ = PQconnectdb(conninfo.c_str());
                if (PQstatus(conn_) != CONNECTION_OK) {
                    throw std::runtime_error{PQerrorMessage(conn_)};
                }
            }

            ~connection() noexcept {
                if (conn_) {
                    PQfinish(conn_);
                    conn_ = nullptr;
                }
            }

            // Prevent copying/moving for safety
            connection(const connection &) = delete;

            connection &operator=(const connection &) = delete;

            PGconn *get() const noexcept { return conn_; }
        };

        class result {
        private:
            void get_metadata() {
                if (!is_ok()) {
                    throw std::runtime_error{error_msg()};
                }
                rows = PQntuples(res_);
                fields = PQnfields(res_);
                fieldname.reserve(fields);
                for (int i{}; i < fields; ++i) fieldname.emplace_back(PQfname(res_, i));
            }

        public:
            explicit result(PGresult *res, PGconn *conn) : res_{res}, conn_(conn) { get_metadata(); }

            ~result() noexcept { if (res_) { PQclear(res_); } }

            // Transfer ownership, clear in destructor
            result &operator=(PGresult *new_res) noexcept {
                if (res_) PQclear(res_);
                res_ = new_res;
                get_metadata();
                return *this;
            }

            bool is_ok() const noexcept {
                return res_ && (PQresultStatus(res_) == PGRES_TUPLES_OK || PQresultStatus(res_) == PGRES_COMMAND_OK);
            }

            std::string error_msg() const noexcept {
                return res_ ? std::string{PQerrorMessage(conn_)} : "No result";
            }

            int row_count() const { return rows; }

            int field_count() const { return fields; }

            std::string field_name(const int field_num) const { return fieldname[field_num]; }

            std::string value(int row, int fieldnum) const { return PQgetvalue(res_, row, fieldnum); }

        private:
            PGresult *res_ = nullptr;
            PGconn *conn_ = nullptr;
            int rows;
            int fields;
            std::vector<std::string> fieldname{};
        };

        class postgres {
        public:
            explicit postgres(std::string conninfo) : conn(conninfo) {
            }

            connection conn;

            result exec(const std::string &query, const std::vector<std::string> &params) {
                if (params.empty()) return result{PQexec(conn.get(), query.c_str()), conn.get()};
                std::vector<const char *> values;
                values.reserve(params.size());
                for (const auto &param: params) values.push_back(param.c_str());
                return result{PQexecParams(conn.get(), query.c_str(), static_cast<int>(values.size()), nullptr, values.data(), nullptr, nullptr, 0), conn.get()};
            }
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
        impl = new pg::postgres(conninfo);
    }

    postgres::~postgres() {
        delete impl;
    }

    postgres::postgres(postgres &&other) noexcept : impl(std::exchange(other.impl, nullptr)) {
    }

    postgres &postgres::operator=(postgres &&other) noexcept {
        if (this != &other) {
            delete impl;
            impl = std::exchange(other.impl, nullptr);
        }
        return *this;
    }

    std::vector<std::map<std::string, std::string> > postgres::exec(const std::string &query, const std::vector<std::string> &parameters) const {
        if (!impl) throw std::logic_error("sc::postgres: exec() on a connection that was moved from");
        const auto res = impl->exec(query, parameters);
        if (!res.row_count()) return {};

        std::vector<std::map<std::string, std::string> > result(res.row_count());
        for (int i = 0; i < res.row_count(); ++i) {
            for (int j = 0; j < res.field_count(); ++j) {
                result[i][res.field_name(j)] = res.value(i, j);
            }
        }
        return std::move(result);
    }
} // sc
