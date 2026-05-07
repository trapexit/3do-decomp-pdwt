#pragma once

#include "3do_types.h"

int32
playdatastream(uint8         *streamName_,
               PlayCPakUserFn userFunction_);
int32
playdatastreams(uint8         *streamName_,
                PlayCPakUserFn userFunction_);
