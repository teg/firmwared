#pragma once

#include <stdio.h>

#define log_info(fmt, arg...) do {				\
		fprintf(stdout, fmt "\n", ##arg);		\
	} while (0)

#define log_warn(fmt, arg...) do {				\
                fprintf(stderr, fmt "\n", ##arg);               \
        } while (0)

#define log_error(fmt, arg...) do {				\
                fprintf(stderr, fmt "\n", ##arg);               \
        } while (0)
