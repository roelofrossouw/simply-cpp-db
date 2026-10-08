// Connects to PostgreSQL and runs a query that needs no schema.
// Servers come from SC_DB_DEMO_SERVER: one server, or several separated by ';'
// ("db1:5432;db2:5433"), tried in order. Unset or empty falls back to 127.0.0.1:5432;
// an invalid value is an error.
// The database, user and password come from SC_DB_DEMO_DBNAME, SC_DB_DEMO_USER and
// SC_DB_DEMO_PASSWORD, defaulting to 1web, www and no password.

#include <core.h>
#include <ip_endpoints.h>
#include <postgres.h>
#include <timer.h>

#include <iostream>

int main() {
    try {
        const sc::ip_endpoints servers{sc::getenv("SC_DB_DEMO_SERVER", "127.0.0.1"), 5432};
        const auto name = sc::getenv("SC_DB_DEMO_DBNAME", "1web");
        const auto user = sc::getenv("SC_DB_DEMO_USER", "www");
        const auto password = sc::getenv("SC_DB_DEMO_PASSWORD");
        std::cout << "PostgreSQL servers: " << servers << '\n';

        // [readme]
        sc::timer sw;
        const sc::postgres db{servers, name, user, password};
        std::cout << "Connected after " << sw << '\n';

        const auto rows = db.exec("select current_database() as database, current_user as user, version() as version");
        for (const auto &row : rows) {
            for (const auto &[field, value] : row) std::cout << field << " = " << value << '\n';
        }
        std::cout << "Done after " << sw << '\n';
        // [/readme]
    } catch (const std::exception &error) {
        std::cerr << "sc-db-demo: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
