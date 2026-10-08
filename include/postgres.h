#pragma once

#include <ip_endpoint.h>

#include <string>
#include <vector>
#include <map>


namespace sc {
    namespace pg {
        class postgres;
    }

    class postgres {
    public :
        postgres(std::string host, std::string db, std::string user, std::string passwd = {});

        // Connects to the first reachable server, in order (libpq's multi-host failover).
        // A server with port 0 uses libpq's default port.
        postgres(const std::vector<ip_endpoint> &servers, const std::string &db, const std::string &user,
                 const std::string &passwd = {});

        ~postgres();

        std::vector<std::map<std::string, std::string> > exec(const std::string &query, const std::vector<std::string> &parameters = {}) const;

    private :
        pg::postgres *impl;
    };
} // sc
