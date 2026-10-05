#ifndef SC_POSTGRES_H
#define SC_POSTGRES_H

#include <string>
#include <vector>
#include <map>


namespace sc {
    namespace pg {
        class postgres;
    }

    class postgres {
    public :
        postgres(std::string host, std::string db, std::string user, std::string passwd = {});

        ~postgres();

        std::vector<std::map<std::string, std::string> > exec(const std::string &query, const std::vector<std::string> &parameters = {}) const;

    private :
        pg::postgres *impl;
    };
} // sc

#endif //SC_POSTGRES_H
