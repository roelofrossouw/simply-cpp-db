// Connects to PostgreSQL and runs two queries that need no tables: one with a parameter, one
// returning several rows. Each step shows the call, as written, and what it returned.
// Everything comes from the standard libpq environment: PGHOST and PGPORT (or the local
// socket), PGDATABASE, PGUSER and PGPASSWORD (or ~/.pgpass).

#include <sc.h>
#include <postgres.h>

#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using rows = std::vector<std::map<std::string, std::string>>;

    // One step of the demo: the call, as written in the source, and the rows it returned.
    void show(const std::string_view call, const rows &result) {
        std::cout << "  " << call << "\n      -> " << result.size() << (result.size() == 1 ? " row" : " rows") << '\n';
        for (const auto &row: result) {
            std::cout << "         {";
            bool first = true;
            for (const auto &[field, value]: row) {
                std::cout << (first ? "" : ", ") << field << ": \"" << value << '"';
                first = false;
            }
            std::cout << "}\n";
        }
    }

    void heading(const std::string_view title) { std::cout << '\n' << title << '\n'; }

    void setting(const char *variable) {
        std::cout << "  " << variable << '=' << sc::getenv(variable, "(not set)") << '\n';
    }
}

#define SHOW(expression) show(#expression, expression)

int main() {
    try {
        std::cout << "simply-cpp db: querying PostgreSQL, each call with what it returned\n"
                  << "Connection settings, from libpq's environment:\n";
        for (const char *variable: {"PGHOST", "PGPORT", "PGDATABASE", "PGUSER"}) setting(variable);
        std::cout << "  PGPASSWORD " << (sc::getenv("PGPASSWORD").empty() ? "(not set)" : "is set") << '\n';
        sc::timer sw;

        // [readme]
        heading("Connecting");
        const sc::postgres db; // host, database, user and password from PGHOST etc.
        std::cout << "  const sc::postgres db;\n      -> connected in " << sw << '\n';

        heading("A query with a parameter ($1 is passed separately, never pasted into the SQL)");
        const std::string query = "select current_database() as database, current_user as user, "
                                  "$1::text as greeting";
        SHOW(db.exec(query, {"Hello World!"}));

        heading("Several rows");
        SHOW(db.exec("select * from (values (1, 'one'), (2, 'two'), (3, 'three')) as t(number, name)"));
        // [/readme]

        std::cout << "\nAll of that took " << sw << ".\n";
    } catch (const std::exception &error) {
        std::cerr << "sc-db-demo: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
