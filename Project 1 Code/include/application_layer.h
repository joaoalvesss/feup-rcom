#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

#include "link_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename);

#endif // _APPLICATION_LAYER_H_
