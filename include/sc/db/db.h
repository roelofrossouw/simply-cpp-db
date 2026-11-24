#pragma once

#include <sc/net/ip_endpoints.h>

#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>


namespace sc::db {
    class connection;

    // A PostgreSQL connection (sc::postgres until 3.0.0).
    class client {
    public :
        // servers is one server or several: "db1;db2:5433", a std::vector<ip_endpoint> or a braced
        // list. libpq connects to the first reachable one, in order (multi-host failover).
        // Anything left empty is up to libpq's own defaults: no servers means PGHOST/PGPORT (or
        // the local socket), a server without a port means PGPORT (or 5432), and an empty db, user
        // or passwd means PGDATABASE, PGUSER and PGPASSWORD (or ~/.pgpass). So sc::db::client db;
        // connects purely from the standard libpq environment. options are further libpq keywords
        // and their values, such as {{"connect_timeout", "3"}, {"sslmode", "require"}} (an empty
        // value is left out, as above).
        explicit client(const ip_endpoints &servers = {}, const std::string &db = {}, const std::string &user = {},
                          const std::string &passwd = {}, const std::map<std::string, std::string> &options = {});

        ~client();

        // A connection can be moved but not copied. A moved-from client throws std::logic_error
        // from exec().
        client(const client &) = delete;
        client &operator=(const client &) = delete;
        client(client &&other) noexcept;
        client &operator=(client &&other) noexcept;

        std::vector<std::map<std::string, std::string> > exec(const std::string &query, const std::vector<std::string> &parameters = {}) const;

        // A NOTIFY on a channel this connection LISTENs to (exec("listen my_channel")).
        struct notification {
            std::string channel;
            std::string payload;
            int pid = 0; // the server process that sent it
        };

        // The next notification: one already received, or the first to come within timeout;
        // nothing when none came. Throws std::runtime_error when the connection is lost (a new
        // client, LISTEN again, is the way back).
        std::optional<notification> wait_notification(std::chrono::milliseconds timeout) const;

    private :
        std::unique_ptr<connection> impl;
    };
} // sc::db
