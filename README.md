# simply-cpp-db

C++20 wrapper around libpq for PostgreSQL, other databases to follow.

The public API uses the `sc` namespace, in the same style as `simply-cpp`.

## Install

### Homebrew (macOS)

```bash
brew tap roelofrossouw/sc
brew install simply-cpp-db
```

### apt (Ubuntu)

```bash
sudo curl -fsSL https://apt.roelof.co.za/setup.sh | bash
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

- **PostgreSQL** - `postgresql-server-dev-all` on apt, `postgresql` on brew; installed automatically if missing when building from source. On macOS this uses whichever Postgres keg `pg_config` resolves to.

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
