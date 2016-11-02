#include "runner.h"

#include <iostream>

#include <crete/exception.h>

using namespace std;
using namespace crete;

int main(int argc, char* argv[])
{
    try
    {
        Runner runner(argc, argv);
    }
    catch(...)
    {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        return -1;
    }

	return 0;
}
