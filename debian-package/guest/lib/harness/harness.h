#ifndef CRETE_HARNESS_CPP_H
#define CRETE_HARNESS_CPP_H

#include <string>
#include <vector>

namespace crete
{

const std::string PROC_MAPS_FILE_NAME = "proc-maps.log";

//struct ProcMapSort
//{
//    bool operator()(const ProcMap& pm1, const ProcMap& pm2)
//    {
//        return pm1.address().first < pm2.address().first;
//    }
//};

class Harness
{
public:
    Harness(std::string exe_name);

protected:
    void update_proc_maps(); // Terminates program if proc-map file isn't found (to give crete-run the proc-map info initially).

private:
    std::string exe_name_;
};

}

#endif // CRETE_HARNESS_CPP_H
