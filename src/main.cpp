#include "audiorecorder.h"
#include <debug.h>

int main(int argc, char **argv)
{
    AudioRecorder ar(argc, argv);

    if (ar.isInited())
        ar.record();

    return 0;
}
