// Standalone: verifies flushed evidence survives process exit without destructors.
#include "../engine/diagnostics.hpp"
#include <cstdlib>
int main(int argc,char** argv) {
    if(argc!=2 || !ra2engine::diag::start(argv[1]))return 2;
    ra2engine::diag::note("ABRUPT_EXIT evidence-before-termination");
    std::_Exit(17);
}
