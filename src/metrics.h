#ifndef METRICS_H
#define METRICS_H

#include "ping.h"

void write_prometheus_metrics(struct ping_stats* stats_array, int count);

#endif
