#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "shell.h"
#include "parser.h"
#include "hashtable.h"
#include "utility.h"

void _test_(const char* string)
{
	struct Shell* shell = start();

	fprintf(stdout, "Input: %s\n", string);
	char* test = copy_string(string);

	initialize(shell, test);

	struct AstPipelineList* pipe_list = parse(shell->parser);
	execute(shell, pipe_list);

	stop(shell);

	free(test);
}

int main()
{
	/*Tests0*/
	//_test_("");
	//_test_("1234567890");
	//_test_("ksdmksmfiookosfmjdsjfiewkfjknfgirewnj");
	//_test_("./test test0");
	//_test_("ls -b");
	//_test_("shitstisjisjtios sjiotjisjtjisj00");

	/*Tests1*/
	//_test_("''");
	//_test_("'exec'");
	//_test_("'0943024302o4i40op2iop3i4op3i3op24oi4o32i4op4'");
	//_test_("'1234567890' '1234567890'");
	//_test_("dkkfdkfld '1234567890'");
	//_test_("'1234567890' rejiowjriewr");
	//_test_("123'456789'");
	//_test_("123'456'789");
	//_test_("'123'456789");
	//_test_("123'45'67'89'0");
	//_test_("'123''456'");
	//_test_("123'456'789'0'		'012'345'678''90'");
	//_test_("'shdj'3829'ajij'39203902'sdsdksd'jskdjsld'2903902'					'323213'321321312'3213213123''12313'");

	/*Tests2*/
	//_test_("exec 123 1234 12345 123456");
	//_test_("'1231' '3434' 434'4343243'  4'3432'43'24324' '3243242'");
	//_test_("'3232' '1' '2' 3 4 '5'");

	/*Tests3*/
	//_test_("=");
	//_test_("var=");
	//_test_("var=6");
	//_test_("var= cmd_name");
	//_test_("var=ieofjejerirjeirjierjierj");
	//_test_("var=2 var0=3\tvar1=4\tvar2=5");
	//_test_("var=2     echo 000");
	//_test_("var=5 var=7");
	//_test_("var=2 var0=3     ec'h'o 03'-403'");
	//_test_("===");

	/*Test4*/
	//_test_("command >file.txt");
	//_test_("comamnd>file.txt");
	//_test_("command>file1.txt<file2.txt");
	//_test_("command > file1.txt < file2.txt");
	//_test_("command arg1 'arg2' >file.txt arg3 '>' <fil0.txt");
	//_test_("command > test1.txt > test2.txt > test3.txt < input.txt");
	//_test_("command > test1.txt > 'test2.txt' < ekjwn'kwerojkj'rewjre");
	//_test_("command >");
	//_test_("command > <");
	//_test_("command > '>'.txt");

	/*Test5*/
	//_test_("$");
	//_test_("$variable");
	//_test_("var1=4 var2=$var1");
	//_test_("cmd_name=echo $cmd_name");
	//_test_("cmd_name=echo $cmd_name test");
	//_test_("cmd_name=echo arg1=test arg2=$test $cmd_name $arg1 $arg2");
	//_test_("var=3 echo '$var'");
	//_test_("var=abcd 'cmd_name'$var");
	//_test_("var=abcd var2=poiu cmd_name $var$var2");
	//_test_("input=input.txt output=output.txt cmd_name=command arg=argument $cmd_name >$output $arg < $input");
	//_test_("input=input.txt output=output.txt cmd_name=command arg=argument '$cmd_name' >$'output' $'arg' < $'input'");
	//_test_("$37847384$");
	//_test_("echo $");
	//_test_("$$$$$$");

	/*Test6*/
	//_test_("var=$((2*/3))");
	//_test_("var=$((eif))");
	//_test_("var=$((2/3)))");
	//_test_("var=5 $((3+2))");
	//_test_("$((1+1))");
	//_test_("cmd_name $((4+2))");
	//_test_("cmd_name $((2*1 / (2))) $((1 + 2 + 3 - 4 )) ");
	//_test_("var=$((5+6)) cmd_name $var $((2*1-3))");
	//_test_("var=$((3 - 2 +--+12 / (4 * 2 - (4 / 2)) * 10 + (((-(1))))))");
	//_test_("var=$((4/2 -     1   + (-6 / +- 5))) cmd_name $((3- 2+ 1- (-(-(-(4)))) * 3 / 2))");

	/*Test7*/
	//_test_("var=$((2+3-1)) cmd_name $((1 + $var - 5))");
	//_test_("var=$((2+3-1)) var=$(($var)) cmd_name $((1 + $var * $var -(-(($var)))))");
	//_test_("var=$((2+3-1)) var0=$((1 + $var - -1 * $var)) cmd_name $((1 + $var - 5 + $var0))");

	/*Test8*/
	//_test_("cmd");
	//_test_("cmd1| cmd2");
	//_test_("cmd1 |cmd2");
	//_test_("cmd1 arg1|cmd2 arg2");
	//_test_("var=$((2 + 3)) cmd_name arg1 $var | \n var2=3 cmd_name2 arg arg2 arg3 $var2 $var");
	//_test_("cmd1|cmd2| cmd3 |   cmd4   |cmd5 | \n\n\n\n\n cmd6 |cmd7 | \n cmd8 |\t\n cmd9");
	//_test_("cmd_name \n | cmd_name2");

	/*Test9*/
	_test_("cmd");
	_test_("cmd\n");
	_test_("\ncmd\n");
	_test_("cmd1 \n cmd2");
	_test_("cmd1 | cmd2\n cmd3 \n cmd4");
	_test_("var=$((2+1)) cmd_name >output.txt $var <input.txt | cmd1 arg1 arg2 arg3\n cmd2 $var >output.txt |test\n cmd\n");
	_test_("cmd1 & cmd2");
	_test_("cmd1;cmd2");
	_test_("cmd1& cmd2;");
	_test_("cmd1; cmd2;");
	_test_("cmd1 | cmd2& cmd3;cmd4");
	_test_("var=0\nvar0=1\ncmd1 arg1 arg2 $var | cmd2 $var0\nvar=$var0\ncmd3|cmd4 &\n");
	_test_("cmd&\ncmd0;");
	_test_("cmd&cmd2;cmd3;cm4;cmd5 | cmd6;cm7&");

	return 0;
}