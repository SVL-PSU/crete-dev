#ifndef CRETE_COVERAGE
#define CRETE_COVERAGE

#include <string>

#include <boost/property_tree/ptree.hpp>

#include <crete/test_case.h>

namespace crete
{

class Coverage
{
public:
    Coverage(const boost::property_tree::ptree& properties);

    void generate(const TestCase& tc);
    void clean();

private:
    boost::property_tree::ptree properties_;
    std::string target_binary_;
};

}

#endif // CRETE_COVERAGE
