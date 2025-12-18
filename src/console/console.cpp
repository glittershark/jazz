#include "daisy_seed.h"
#include <stdio.h>
#include <string.h>

using namespace daisy;

static DaisySeed hw;

int main(void) {
    hw.Configure();
    hw.Init();
    hw.StartLog();

    while (1) {
        System::Delay(500);
        hw.PrintLine("test thing: %s", "string");
    }
}
