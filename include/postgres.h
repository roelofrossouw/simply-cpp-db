#pragma once

#include <ip_endpoints.h>

#include <string>
#include <vector>
#include <map>


namespace sc {
    namespace pg {
        class postgres;
    }

    class postgres {
    public :
        // servers is one server or several: "db1;db2:5433", a std::vector<ip_endpoint> or a braced
        // list. libpq connects to the first reachable one, in order (multi-host failover). A
        // server without a port uses libpq's default port.
        postgres(const ip_endpoints &servers, const std::string &db, const std::string &user,
                 const std::string &passwd = {});

        ~postgres();

        std::vector<std::map<std::string, std::string> > exec(const std::string &query, const std::vector<std::string> &parameters = {}) const;

    private :
        pg::postgres *impl;
    };
} // sc
