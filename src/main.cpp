#include <Core/Firmament.h>

using namespace CZ;

int main()
{
    Firmament firmament;

    if (!firmament.marco || !firmament.xdgkit || !firmament.heaven)
        return 1;

    return firmament.marco->run();
}
