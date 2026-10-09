// Connects to PostgreSQL and runs two queries that need no tables: one with a parameter, one
// returning several rows. Each step shows the call, as written, and what it returned.
// Everything comes from the standard libpq environment: PGHOST and PGPORT (or the local
// socket), PGDATABASE, PGUSER and PGPASSWORD (or ~/.pgpass).

#include <sc.h>
#include <postgres.h>

#include <iostream>
#include <string>

int main() {
    try {
        sc::console::title("simply-cpp db: querying PostgreSQL, each call with what it returned");
        sc::console::output() << "Connection settings, from libpq's environment:\n";
        for (const char *variable: {"PGHOST", "PGPORT", "PGDATABASE", "PGUSER"}) {
            sc::console::note(std::string{variable} + '=' + sc::getenv(variable, "(not set)"));
        }
        sc::console::note(sc::getenv("PGPASSWORD").empty() ? "PGPASSWORD (not set)" : "PGPASSWORD is set");
        sc::timer sw;

        // [readme]
        sc::console::heading("Connecting");
        const sc::postgres db; // host, database, user and password from PGHOST etc.
        sc::console::show_text("const sc::postgres db;", "connected in " + std::string(sw));

        sc::console::heading("A query with a parameter ($1 is passed separately, never pasted into the SQL)");
        const std::string query = "select current_database() as database, current_user as user, "
                                  "$1::text as greeting";
        SC_SHOW(db.exec(query, {"Hello World!"}));

        sc::console::heading("Several rows");
        SC_SHOW(db.exec("select * from (values (1, 'one'), (2, 'two'), (3, 'three')) as t(number, name)"));
        // [/readme]

        sc::console::output() << "\nAll of that took " << sw << ".\n";
    } catch (const std::exception &error) {
        std::cerr << "sc-db-demo: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
