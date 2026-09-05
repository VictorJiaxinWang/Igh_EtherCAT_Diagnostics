#include "ethercat_diag/infrastructure/runtime_output.h"

#include <ostream>

void enableImmediateFlush(std::ostream& output)
{
    output << std::unitbuf;
}
