#include "ethercat_diag/infrastructure/runtime_output.h"

#include <cassert>
#include <ios>
#include <sstream>

int main()
{
    std::ostringstream output;

    assert(
        (output.flags() & std::ios_base::unitbuf) == 0);

    enableImmediateFlush(output);

    assert(
        (output.flags() & std::ios_base::unitbuf) != 0);

    return 0;
}
