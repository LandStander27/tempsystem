#pragma once
#include <stdio.h>
#include <string>

static inline void print_error(const std::string message, const unsigned int line = __builtin_LINE(), const char* file = __builtin_FILE()) {
	fprintf(stderr, "\033[31merror at %s:%d:\n\t%s\n\033[39m", file, line, message.c_str());
}

static inline void print_info(const std::string message) {
	printf("LOG: %s\n", message.c_str());
}
