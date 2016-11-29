#pragma once

#include <stdbool.h>

int firmware_load(int devicefd, int firmwarefd, bool tentative);
int firmware_cancel_load(int devicefd);
