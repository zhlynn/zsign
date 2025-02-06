#pragma once
#include <string.h>
#include "getopt.h"

int		optind = 1;
int		opterr = 1;
int		optopt = 0;
char*	optarg = NULL;

int getopt_long(int argc, char* const argv[], const char* shortopts, const struct option* longopts, int* longindex)
{
	if (optind >= argc) {
		return -1;
	}
	char* arg = argv[optind];
	if ('-' != arg[0]) {
		return -1;
	}
	if (0 == strcmp(arg, "--")) {
		optind++;
		return -1;
	}
	if (0 == strcmp(arg, "-")) {
		optind++;
		return -1;
	}
	if (0 == strncmp(arg, "--", 2)) {
		char* value = strchr(arg, '=');
		if (NULL != value) {
			*value = 0;
		}
		for (int i = 0; NULL != longopts[i].name; i++) {
			if (0 == strcmp(arg + 2, longopts[i].name)) {
				if (required_argument == longopts[i].has_arg) {
					if (NULL != value) {
						optarg = value + 1;
					} else {
						optind++;
						if (optind >= argc) {
							return -1;
						}
						optarg = argv[optind];
					}
				} else {
					optarg = NULL;
				}
				optind++;
				return longopts[i].val;
			}
		}
	} else if (0 == strncmp(arg, "-", 1)) {
		const char* p = strchr(shortopts, arg[1]);
		if (NULL == p) {
			return -1;
		}
		if (':' == p[1]) {
			optind++;
			if (optind >= argc) {
				return -1;
			}
			optarg = argv[optind];
		} else {
			optarg = NULL;
		}
		optind++;
		return arg[1];
	}
	return -1;
}
