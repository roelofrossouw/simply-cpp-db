## TESTS ##
# What every test in this directory links against.
set(SC_TEST_LINK_LIBRARIES sc-db)

# Tests labelled "database" need a reachable PostgreSQL server; run only the rest with
#     ctest -LE database
add_sc_test(postgres LABELS database TIMEOUT 120)
