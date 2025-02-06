#pragma once

#define no_argument			0
#define required_argument	1
#define optional_argument	2

struct option
{
	const char* name;
	int			has_arg;
	int*		flag;
	int			val;
};

extern int		optind;
extern int		opterr;
extern int		optopt;
extern char*	optarg;

int getopt_long(int argc, char* const argv[], const char* shortopts, const struct option* longopts, int* longindex);
