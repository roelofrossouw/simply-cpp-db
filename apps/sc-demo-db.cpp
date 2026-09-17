#include <iostream>
#include <postgres.h>

using namespace std;

int main() {
    sc::postgres dev("devdb", "1web", "www");
    auto table = dev.exec("select id, friendlyname(id) as fname, surname from person where surname is not null limit 10");
    for (const auto &row: table) {
        for (const auto &[field, value]: row) {
            cout << "<" << field << ":" << value << "> ";
        }
        cout << "\n";
    }
    return 0;
}
