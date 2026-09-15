#include <iostream>
#include <postgres.h>

using namespace std;

int main() {
    sc::postgres dev{"devdb", "1web", "www"};
    auto rslt = dev.exec("select id, friendlyname(id), idno from person where idno is not null limit 6;");

    for (const auto &row: rslt) {
        cout << row.at("id") << " - ";
        for (const auto &[field, value]: row) {
            cout << "<" << field << "> = " << value << ", ";
        }
        cout << "\n";
    }

    return 0;
}
