// Connects to PostgreSQL and runs a query with a parameter that needs no schema.
// Everything comes from the standard libpq environment: PGHOST and PGPORT (or the local
// socket), PGDATABASE, PGUSER and PGPASSWORD (or ~/.pgpass).

#include <sc.h>
#include <postgres.h>

int main() {
    try {
        // [readme]
        sc::timer sw;
        const sc::postgres db; // database, user and password from PGDATABASE etc.
        std::cout << "Connected after " << sw << '\n';

        const std::string query = R"(
            select current_database() as database,
                   current_user as user,
                   version() as version,
                   $1::text as param1
        )";
        const auto rows = db.exec(query, {"First Parameter"});
        for (const auto &row: rows) {
            for (const auto &[field, value]: row)
                std::cout << field << " = " << value << '\n';
        }
        std::cout << "Done after " << sw << '\n';
        // [/readme]
    } catch (const std::exception &error) {
        std::cerr << "sc-db-demo: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
