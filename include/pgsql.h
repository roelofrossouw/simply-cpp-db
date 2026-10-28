#pragma once

#include <ip_endpoints.h>

#include <string>
#include <vector>
#include <map>
#include <memory>


namespace sc {
    namespace pg {
        class connection;
    }

    class postgres {
    public :
        // servers is one server or several: "db1;db2:5433", a std::vector<ip_endpoint> or a braced
        // list. libpq connects to the first reachable one, in order (multi-host failover).
        // Anything left empty is up to libpq's own defaults: no servers means PGHOST/PGPORT (or
        // the local socket), a server without a port means PGPORT (or 5432), and an empty db, user
        // or passwd means PGDATABASE, PGUSER and PGPASSWORD (or ~/.pgpass). So sc::postgres db;
        // connects purely from the standard libpq environment. options are further libpq keywords
        // and their values, such as {{"connect_timeout", "3"}, {"sslmode", "require"}} (an empty
        // value is left out, as above).
        explicit postgres(const ip_endpoints &servers = {}, const std::string &db = {}, const std::string &user = {},
                          const std::string &passwd = {}, const std::map<std::string, std::string> &options = {});

        ~postgres();

        // A connection can be moved but not copied. A moved-from postgres throws std::logic_error
        // from exec().
        postgres(const postgres &) = delete;
        postgres &operator=(const postgres &) = delete;
        postgres(postgres &&other) noexcept;
        postgres &operator=(postgres &&other) noexcept;

        std::vector<std::map<std::string, std::string> > exec(const std::string &query, const std::vector<std::string> &parameters = {}) const;

    private :
        std::unique_ptr<pg::connection> impl;
    };
} // sc
