// Connects to PostgreSQL and runs a query that needs no schema.
// Servers come from SC_DB_DEMO_SERVER: one server, or several separated by ';'
// ("db1:5432;db2:5433"), tried in order; an invalid value is an error. Everything else uses
// the standard libpq environment: PGDATABASE, PGUSER and PGPASSWORD (or ~/.pgpass), and
// PGHOST/PGPORT (or the local socket) when SC_DB_DEMO_SERVER is unset or empty.

#include <core.h>
#include <ip_endpoints.h>
#include <postgres.h>
#include <timer.h>

#include <iostream>

int main() {
    try {
        const sc::ip_endpoints servers{sc::getenv("SC_DB_DEMO_SERVER")};
        std::cout << "PostgreSQL servers: " << (servers.empty() ? "libpq default (PGHOST)" : servers.to_string()) << '\n';

        // [readme]
        sc::timer sw;
        const sc::postgres db{servers};  // database, user and password from PGDATABASE etc.
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
