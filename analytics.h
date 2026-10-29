#ifndef ANALYTICS_H
#define ANALYTICS_H

#include "mos.h"

void analytics_write_report(
    const MOSState *s,
    const char *filename
);

#endif