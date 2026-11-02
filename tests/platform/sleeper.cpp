// A harmless stand-in for a "distracting app" in the platform tests: it just waits to be closed.
#include <windows.h>

int main() {
    Sleep(60'000);
    return 0;
}
