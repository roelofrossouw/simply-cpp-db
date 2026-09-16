#include <postgres.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#include <sc_test.h>

using namespace std;

namespace {
    // The live checks run against whatever server these point at, so a different
    // machine can redirect them without editing the test.
    string setting(const char *name, const char *fallback) {
        const char *value = getenv(name);
        return value && *value ? value : fallback;
    }

    bool has_field(const map<string, string> &row, const string &field) { return row.find(field) != row.end(); }
}

int main() {
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
    }

    SECTION("Live server");
    {
        const auto host = setting("SC_DB_TEST_HOST", "devdb");
        const auto name = setting("SC_DB_TEST_NAME", "1web");
        const auto user = setting("SC_DB_TEST_USER", "www");
        const auto password = setting("SC_DB_TEST_PASSWORD", "");

        bool reachable = true;
        try {
            const sc::postgres probe{host, name, user, password};
        } catch (const runtime_error &) {
            reachable = false;
        }

        if (!reachable) {
            cout << "   (no PostgreSQL at " << user << '@' << host << '/' << name
                 << ", live checks not run - set SC_DB_TEST_HOST etc. to redirect)" << endl;
        } else {
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
        }
    }

    TEST_SUMMARY();
}
