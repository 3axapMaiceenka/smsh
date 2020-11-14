#include "parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char* copy_string(const char* string)
{
	size_t size = sizeof(char) * (strlen(string) + 1);
	char* copy = malloc(size);
	memcpy(copy, string, size);

	return copy;
}

void _test_(const char* string)
{
	fprintf(stdout, "Input: %s\n", string);
	char* test = copy_string(string);

	struct Parser parser;
	initialize(&parser, test);
	struct AstSimpleCommand* sc = parse(&parser);
	execute(sc, test);

	free(test);
}

int main(int argc, char** argv)
{
	/*Tests0*/
	_test_("");
	_test_("1234567890");
	_test_("ksdmksmfiookosfmjdsjfiewkfjknfgirewnj");
	_test_("./test test0");
	_test_("shitstisjisjtios sjiotjisjtjisj00");

	/*Tests1*/
	_test_("''");
	_test_("'exec'");
	_test_("'0943024302o4i40op2iop3i4op3i3op24oi4o32i4op4'");
	_test_("'1234567890' '1234567890'");
	_test_("dkkfdkfld '1234567890'");
	_test_("'1234567890' rejiowjriewr");
	_test_("123'456789'");
	_test_("123'456'789");
	_test_("'123'456789");
	_test_("123'45'67'89'0");
	_test_("'123''456'");
	_test_("123'456'789'0'		'012'345'678''90'");
	_test_("'shdj'3829'ajij'39203902'sdsdksd'jskdjsld'2903902'					'323213'321321312'3213213123''12313'");

	return 0;
}