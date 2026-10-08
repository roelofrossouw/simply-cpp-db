# simply-cpp-db

C++20 wrapper around libpq for PostgreSQL, other databases to follow.

The public API uses the `sc` namespace, in the same style as `simply-cpp`.

## Install

### Homebrew (macOS)

```bash
curl -fsSL https://apt.roelof.co.za/setup.sh | bash # taps roelofrossouw/sc - same command as the apt one below
brew install simply-cpp-db
```

### apt (Ubuntu)

```bash
curl -fsSL https://apt.roelof.co.za/setup.sh | bash # registers the apt repo - same command as the brew one above
sudo apt -y install simply-cpp-db-dev
```

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
        sc-db
        GIT_REPOSITORY https://github.com/roelofrossouw/simply-cpp-db.git
        GIT_TAG main # or a specific tag, e.g. v1.0.5, to stay stable
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(sc-db)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE sc::sc-db)
```

### Git submodule

```bash
git submodule add https://github.com/roelofrossouw/simply-cpp-db.git third_party/sc-db
```

```cmake
add_subdirectory(third_party/sc-db)
target_link_libraries(myapp PRIVATE sc::sc-db)
```

## Dependencies

sc-db does **not** depend on `simply-cpp` (sc-core) or any other `sc-*` module - it only needs PostgreSQL:

- **PostgreSQL client library (libpq)** - `postgresql-server-dev-all` on apt, `libpq` on brew; installed automatically if missing when building from source. The Homebrew dependency is the client-only package, avoiding a full PostgreSQL server install. On macOS this uses whichever Postgres keg `pg_config` resolves to.

## Usage

```cmake
find_package(sc-db CONFIG REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE sc::sc-db)
```

```cpp
#include <postgres.h>
#include <iostream>

int main() {
    sc::postgres db("host", "dbname", "user");
    auto table = db.exec("select id, name from person limit 10");
    for (const auto &row : table) {
        for (const auto &[field, value] : row) {
            std::cout << "<" << field << ":" << value << "> ";
        }
        std::cout << "\n";
    }
}
```

Every argument is optional. Anything left empty is up to libpq's own defaults,
so the standard `PGHOST`, `PGPORT`, `PGDATABASE`, `PGUSER` and `PGPASSWORD`
variables (and `~/.pgpass`) apply, and `sc::postgres db;` connects purely from
them. Explicit arguments win over the environment.

The first argument is an `sc::ip_endpoints`, so it can be a `host[:port]`
string, several separated by `;`, a `std::vector<sc::ip_endpoint>` or a braced
list. With several servers libpq connects to the first one that answers; a
server without a port uses the default port:

```cpp
sc::postgres db("db1;db2:5433", "dbname", "user", "password");
sc::postgres listed({{"db1", 5432}, {"db2", 5433}}, "dbname", "user", "password");
```

## Demo

`sc-db-demo` is installed with the runtime package (`simply-cpp-db`), so you can
check a machine can reach PostgreSQL without installing the `-dev` package:

```bash
PGDATABASE=mydb PGUSER=me sc-db-demo                    # local socket
PGHOST=db1.example.com,db2.example.com PGPORT=5432,5433 PGDATABASE=mydb PGUSER=me sc-db-demo
```

Unlike the other demos it has no `SC_DB_DEMO_SERVER`: everything comes from the
standard libpq environment, `PGHOST` and `PGPORT` (or the local socket),
`PGDATABASE`, `PGUSER` and `PGPASSWORD` (or `~/.pgpass`). For several servers,
libpq takes comma-separated `PGHOST` and `PGPORT` lists and tries them in order.
It runs a query with a parameter (`$1`), passed separately from the SQL. The `example-sc-db-demo`
CTest uses the same variables, which build servers get from
`/etc/simply-cpp/test.env`.

Its source is `examples/sc-db-demo.cpp`; the code below is copied from it at
configure time, so it always matches code that compiles:

<!-- sc-example: examples/sc-db-demo.cpp -->
```cpp
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
```
<!-- /sc-example -->

## Requirements

- CMake 3.22 or newer
- A C++20 compiler
- PostgreSQL client/server dev headers (see Dependencies above)

## Building and testing

```bash
cmake -B build -S .
cmake --build build -j
ctest --test-dir build --output-on-failure
```
