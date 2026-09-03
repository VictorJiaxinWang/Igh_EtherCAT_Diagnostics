#include <cstdlib>

int main()
{
#ifdef NDEBUG
    return EXIT_FAILURE;
#else
    return EXIT_SUCCESS;
#endif
}
