#include "postgres.h"

#include <format>
#include <libpq-fe.h>
#include <stdexcept>

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

                char *strings[params.size()];
                for (int i{}; i < params.size(); i++) strings[i] = const_cast<char *>(params[i].c_str());
                return result{PQexecParams(conn.get(), query.c_str(), params.size(), nullptr, strings, nullptr, nullptr, 0), conn.get()};
            }
        };
    }

    postgres::postgres(std::string host, std::string db, std::string user, std::string passwd) {
        impl = new pg::postgres(std::format("host={} dbname={} user={} password={}", host, db, user, passwd));
    }

    postgres::~postgres() {
        delete impl;
    }

    std::vector<std::map<std::string, std::string> > postgres::exec(const std::string &query, const std::vector<std::string> &parameters) const {
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
