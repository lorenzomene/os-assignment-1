#include "helpers.h"
#include <ctype.h>
#include <string.h>

#define HELPERS_H

char *trim_whitespace(char *str) {
	char *end;

	while (isspace((unsigned char)*str)) {
		str++;
	}

	if (*str == 0) {
		return str;
	}

	end = str + strlen(str) - 1;
	
	while (end > str && isspace((unsigned char)*end)) {
		end--;
	}


	*(end + 1) = 0;
	
	return str;
}


