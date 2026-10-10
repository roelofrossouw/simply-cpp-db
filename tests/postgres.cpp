#include <core.h>
#include <pgsql.h>

#include <cstdlib>
#include <optional>
#include <utility>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <sc_test.h>

using namespace std;

namespace {
    bool has_field(const map<string, string> &row, const string &field) { return row.find(field) != row.end(); }

    // Sets libpq environment variables for one scope, restoring the previous values after.
    class pg_environment {
        vector<pair<string, optional<string> > > saved_;

    public:
        explicit pg_environment(const vector<pair<string, string> > &values) {
            for (const auto &[name, value]: values) {
                const char *old = getenv(name.c_str());
                saved_.emplace_back(name, old ? optional<string>{old} : nullopt);
                setenv(name.c_str(), value.c_str(), 1);
            }
        }

        ~pg_environment() {
            for (const auto &[name, value]: saved_) {
                if (value) setenv(name.c_str(), value->c_str(), 1);
                else unsetenv(name.c_str());
            }
        }
    };
}

int main() {
    SECTION("A connection moves but doesn't copy");
    {
        // A copy would share one libpq connection and free it twice.
        static_assert(!std::is_copy_constructible_v<sc::postgres>);
        static_assert(!std::is_copy_assignable_v<sc::postgres>);
        static_assert(std::is_nothrow_move_constructible_v<sc::postgres>);
        static_assert(std::is_nothrow_move_assignable_v<sc::postgres>);
        CHECK(true);
    }

    SECTION("A connection that cannot be made is reported, not returned broken");
    {
        // No server on this host, or a server without that database: either way the
        // constructor has to throw rather than hand back an unusable object.
        CHECK_THROWS_AS((sc::postgres{"127.0.0.1", "sc_db_test_no_such_database", "no_such_user", "no_such_password"}),
                        runtime_error);
        CHECK_THROWS_AS((sc::postgres{"no-such-host.invalid", "postgres", "postgres"}), runtime_error);

        // The failure carries the driver's message rather than an empty string.
        try {
            const sc::postgres unreachable{"no-such-host.invalid", "postgres", "postgres"};
            CHECK_MSG(false, "expected a connection failure");
        } catch (const runtime_error &error) {
            CHECK(!string{error.what()}.empty());
        }

        // The same through a server list.
        CHECK_THROWS_AS((sc::postgres{{{"no-such-host.invalid", 5432}, {"also-no-such-host.invalid", 0}},
                                      "postgres", "postgres"}), runtime_error);
    }

    SECTION("Live server");
    {
        // The live checks run against whatever the standard libpq variables point at, so a
        // different machine can redirect them without editing the test.
        const auto host = sc::getenv("PGHOST", "devdb");
        const auto name = sc::getenv("PGDATABASE", "1web");
        const auto user = sc::getenv("PGUSER", "www");
        const auto password = sc::getenv("PGPASSWORD");

        bool reachable = true;
        try {
            const sc::postgres probe{host, name, user, password};
        } catch (const runtime_error &) {
            reachable = false;
        }

        if (!reachable) {
            cout << "   (no PostgreSQL at " << user << '@' << host << '/' << name
                 << ", live checks not run - set PGHOST, PGDATABASE, PGUSER and PGPASSWORD to redirect)" << endl;
        } else {
            // Moved, the connection keeps working; the moved-from object refuses to be used.
            sc::postgres first{host, name, user, password};
            sc::postgres moved{std::move(first)};
            const auto two = moved.exec("select 2 as two");
            CHECK_EQ(two.at(0).at("two"), string{"2"});
            CHECK_THROWS_AS(first.exec("select 1"), logic_error);
            first = std::move(moved);
            const auto three = first.exec("select 3 as three");
            CHECK_EQ(three.at(0).at("three"), string{"3"});

            const sc::postgres db{host, name, user, password};

            // A single row, addressed by column alias.
            const auto one = db.exec("select 1 as one");
            CHECK_EQ(one.size(), size_t{1});
            if (one.size() == 1) {
                CHECK(has_field(one[0], "one"));
                CHECK_EQ(one[0].at("one"), string{"1"});
            }

            // Several rows and columns, in order, without depending on any schema.
            const auto rows = db.exec("select * from (values (1,'a'),(2,'b'),(3,'c')) as t(num, letter)");
            CHECK_EQ(rows.size(), size_t{3});
            if (rows.size() == 3) {
                CHECK_EQ(rows[0].size(), size_t{2}); // one entry per column
                CHECK_EQ(rows[0].at("num"), string{"1"});
                CHECK_EQ(rows[0].at("letter"), string{"a"});
                CHECK_EQ(rows[2].at("num"), string{"3"});
                CHECK_EQ(rows[2].at("letter"), string{"c"});
                CHECK(has_field(rows[1], "num"));
                CHECK(!has_field(rows[1], "nosuchcolumn"));
            }

            // No rows is an empty vector, not an error and not a row of blanks.
            const auto none = db.exec("select 1 as one where false");
            CHECK(none.empty());

            // Bound parameters are passed through and come back as text.
            const auto echoed = db.exec("select $1::text as echo, $2::int as number", {"hello", "42"});
            CHECK_EQ(echoed.size(), size_t{1});
            if (echoed.size() == 1) {
                CHECK_EQ(echoed[0].at("echo"), string{"hello"});
                CHECK_EQ(echoed[0].at("number"), string{"42"});
            }

            // A parameter is data, never sql: this must come back as a string.
            const auto injected = db.exec("select $1::text as value", {"'; drop table person; --"});
            CHECK_EQ(injected.size(), size_t{1});
            if (injected.size() == 1) CHECK_EQ(injected[0].at("value"), string{"'; drop table person; --"});

            // Several parameters, each in its own place.
            const auto several = db.exec("select $1::text as a, $2::int + 1 as b, $3::text as c", {"x", "41", "z"});
            CHECK_EQ(several.size(), size_t{1});
            if (several.size() == 1) {
                CHECK_EQ(several[0].at("a"), string{"x"});
                CHECK_EQ(several[0].at("b"), string{"42"});
                CHECK_EQ(several[0].at("c"), string{"z"});
            }

            // Nulls come back as empty strings.
            const auto nulls = db.exec("select null::text as nothing");
            CHECK_EQ(nulls.size(), size_t{1});
            if (nulls.size() == 1) CHECK_EQ(nulls[0].at("nothing"), string{});

            // Broken sql throws instead of returning an empty result that looks like
            // "no rows matched".
            CHECK_THROWS_AS(db.exec("select * from a_table_that_does_not_exist"), runtime_error);
            CHECK_THROWS_AS(db.exec("this is not sql"), runtime_error);

            // The connection survives a failed query.
            const auto after = db.exec("select 1 as one");
            CHECK_EQ(after.size(), size_t{1});

            // A server list fails over past a server that can't be reached.
            const sc::postgres failover{{{"no-such-host.invalid", 5432}, {host, 0}}, name, user, password};
            const auto via_list = failover.exec("select 1 as one");
            CHECK_EQ(via_list.size(), size_t{1});

            // Anything not given comes from libpq's environment variables.
            {
                const pg_environment environment{{{"PGHOST", host}, {"PGDATABASE", name}, {"PGUSER", user}}};
                const auto who = "select current_database() as db, current_user as usr";
                const sc::postgres from_environment;
                const auto all = from_environment.exec(who);
                CHECK_EQ(all.size(), size_t{1});
                if (all.size() == 1) {
                    CHECK_EQ(all[0].at("db"), name);
                    CHECK_EQ(all[0].at("usr"), user);
                }
                // A server given, database and user still from the environment.
                const sc::postgres server_only{host};
                CHECK_EQ(server_only.exec(who).size(), size_t{1});
            }
            {
                // An explicit value wins over the environment.
                const pg_environment environment{{{"PGDATABASE", "sc_db_test_no_such_database"}}};
                const sc::postgres explicit_db{host, name, user, password};
                const auto current = explicit_db.exec("select current_database() as db");
                CHECK_EQ(current.size(), size_t{1});
                if (current.size() == 1) CHECK_EQ(current[0].at("db"), name);
                CHECK_THROWS_AS((sc::postgres{host, "", user, password}), runtime_error);
            }

            // The same as one ';'-separated string.
            const sc::postgres from_string{"no-such-host.invalid;" + host, name, user, password};
            CHECK_EQ(from_string.exec("select 1 as one").size(), size_t{1});
        }
    }

    TEST_SUMMARY();
}
