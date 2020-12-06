#include "builtin.h"
#include "utility.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

extern char** environ;

//cd [-L |-P] [directory]
static int cd(struct Shell* shell, char** argv);

// export [name] [value] (without '=')
static int export(struct Shell* shell, char** argv);

// unset [name]
static int unset(struct Shell* shell, char** argv);

// help [bulitin_name]
static int help(struct Shell* shell, char** argv);

static const struct Builtin Builtins[] = 
{
	{ "cd", cd }, 
	{ "export", export }, // add environment variable
	{ "unset", unset }, // remove environment variable
	{ "help", help }
};

static const size_t BuiltinsCount = sizeof(Builtins) / sizeof(struct Builtin); 

const struct Builtin* const is_builtin(const char* name)
{
	if (name)
	{
		for (size_t i = 0; i < BuiltinsCount; i++)
		{
			if (!strcmp(name, Builtins[i].name))
			{
				return Builtins + i;
			}
		}
	}

	return NULL;
}

static int is_directory(const char* path)
{
	struct stat sb;

	if (stat(path, &sb) == -1)
	{
		return 0;
	}

	return S_ISDIR(sb.st_mode);
}

// cd [-L |-P] [directory]
static int cd(struct Shell* shell, char** argv)
{
	char* directory = NULL;
	int option = 0; // -L - 0, -P - 1

	if (*argv)
	{
		for (int running = 1; *argv && running; )
		{
			if (!strcmp(*argv, "-L"))
			{
				option = 0;
				argv++;
			}
			else
			{
				if (!strcmp(*argv, "-P"))
				{
					option = 1;
					argv++;
				}
				else
				{
					running = 0;
				}
			}
		}
	}
	
	if (*argv)
	{
		if (*(++argv))
		{
			fprintf(stderr, "cd: too many arguments\n");
			return 1;
		}
		else
		{
			directory = copy_string(*(--argv));
		}
	}
	else
	{
		const char* home = get_variable(shell, "HOME");

		if (!home || home[0] == '\0')
		{
			return 0;
		}

		directory = copy_string(home);
	}

	char* curpath = NULL;

	if (directory[0] == '/' || (directory[0] == '.' && (directory[1] == '/' || (directory[1] == '.' && directory[2] == '/'))))
	{
		curpath = copy_string(directory);
	}
	else
	{
		const char* cdpath = get_variable(shell, "CDPATH");

		if (!cdpath)
		{
			char* path = concat_strings("./", directory);

			if (is_directory(path))
			{
				curpath = path;
			}
			else
			{
				free(path);
				curpath = copy_string(directory);
			}
		}
		else
		{

			const size_t dir_len = strlen(directory);
			const char* _cdpath = strchr(cdpath, ':');
			const char* prev = cdpath;
			char* path = NULL;
			int result = 0;

			while (1)
			{
				size_t path_part = (size_t)(_cdpath - prev);
				size_t slash = *(_cdpath - 1) != '/' ? 1 : 0;
				size_t size = dir_len + path_part + slash + 1;

				path = realloc(path, size);

				snprintf(path, path_part + 1, "%s", prev);

				if (slash)
				{
					path[path_part++] = '/';
				}

				snprintf(path + path_part, dir_len + 1, "%s", directory);
				path[size - 1] = '\0';

				if (is_directory(path))
				{
					curpath = path;
					result = 1;
					break;
				}

				if (*_cdpath == '\0')
				{
					break;
				}

				prev = _cdpath + 1;
				_cdpath = strchr(_cdpath + 1, ':');

				if (!_cdpath) // last iteration
				{
					_cdpath = cdpath + strlen(cdpath);
				}
			}

			if (!result)
			{
				curpath = copy_string(directory);
				free(path);
			}
		}
	}

	if (!option && curpath[0] != '/')
	{
		const char* pwd = get_variable(shell, "PWD");

		size_t pwd_len = strlen(pwd);
		size_t curpath_len = strlen(curpath);
		size_t slash = pwd[pwd_len - 1] != '/' ? 1 : 0;
		size_t len = pwd_len + curpath_len + slash + 1;
		char* _curpath = copy_string(curpath);

		curpath = realloc(curpath, len);

		snprintf(curpath, pwd_len + 1, "%s", pwd);

		if (slash)
		{
			curpath[pwd_len++] = '/';
		}

		snprintf(curpath + pwd_len, curpath_len + 1, "%s", _curpath);
		curpath[len - 1] = '\0';
		free(_curpath);
	}

	int rc = chdir(curpath); 

	if (!rc) // succesful
	{
		char* pwd = copy_string(get_variable(shell, "PWD"));

		if (pwd && pwd[0] != '\0')
		{
			getcwd(curpath, strlen(curpath)); // get current directory in the better readable form

			set_variable(shell, "PWD", curpath);
			set_variable(shell, "OLDPWD", pwd);
		}

		free(pwd);
	}
	else
	{
		fprintf(stderr, "cd: %s: No such file or directory\n", directory);
	}

	free(curpath);
	free(directory);

	return rc == 0 ? rc : 1;
}

// export [name] [value] (without '=')
static int export(struct Shell* shell, char** argv)
{
	if (!*argv)
	{
		fprintf(stderr, "export: no arguments\n");
		return 1;
	}

	const char* var_name = *argv;
	const char* var_value = *(++argv) ? *argv : "";

	if (*argv && *(++argv))
	{
		fprintf(stderr, "export: too many arguments\n");
		return 1;
	}

	if (get_variable(shell, var_name)) // if that variable already exists
	{
		set_variable(shell, var_name, var_value);
		return 0;
	}

	size_t env_count = 0;

	for (char** ptr = environ; *ptr; ptr++)
	{
		env_count++;
	}

	char** new_environ = calloc(env_count + 2, sizeof(char*));

	for (size_t i = 0; i < env_count; i++)
	{
		new_environ[i] = copy_string(environ[i]);
	}

	size_t name_len = strlen(var_name);
	size_t value_len = strlen(var_value);
	size_t len = name_len + value_len + 2;

	new_environ[env_count] = calloc(len, sizeof(char));
	strncat(new_environ[env_count], var_name, name_len);
	new_environ[env_count][name_len] = '=';
	strncat(new_environ[env_count] + name_len + 1, var_value, value_len);

	environ = new_environ;
	set_variable(shell, var_name, var_value);

	return 0;
}

// unset [name]
static int unset(struct Shell* shell, char** argv)
{
	if (!*argv)
	{
		fprintf(stderr, "unset: no arguments\n");
		return 1;
	}

	const char* name = *argv;

	if (*(++argv))
	{
		fprintf(stderr, "unset: too many arguments\n");
		return 1;
	}

	size_t env_count = 0;
	size_t name_len = strlen(name);
	char** env = environ;

	for (; *env; env++)
	{
		if (!strncmp(name, *env, name_len) && (*env)[name_len] == '=')
		{
			break;
		}

		env_count++;
	}

	if (!*env)
	{
		fprintf(stderr, "unset: '%s': not a valid idenfifier\n", name);
		return 1;
	}

	for (char** ptr = env; *ptr; ptr++)
	{
		env_count++;
	}

	char** new_environ = calloc(env_count, sizeof(char*));

	for (size_t i = 0, j = 0; i < env_count; i++)
	{
		if (environ + i != env)
		{
			new_environ[j++] = copy_string(environ[i]);
		}
	}

	environ = new_environ;
	erase(shell->variables, name);

	return 0;
}

// help [bulitin_name]
static int help(struct Shell* shell, char** argv)
{
	if (*argv)
	{
		const char* builtin_name = *argv;

		if (*(++argv))
		{
			fprintf(stderr, "help: too many arguments\n");
			return 1;
		}

		if (!strcmp(builtin_name, "cd"))
		{
			fprintf(stdout, "cd: cd [-L |-P] [directory]\nChanges current directory\n");
			return 0;
		}

		if (!strcmp(builtin_name, "export"))
		{
			fprintf(stdout, "export: export [name] [value]\nCreates [name] environment variable for current shell session\n");
			return 0;
		}

		if (!strcmp(builtin_name, "unset"))
		{
			fprintf(stdout, "unset: unset [name]\nRemoves [name] environment variable for current shell session\n");
			return 0;
		}

		if (!strcmp(builtin_name, "help"))
		{
			fprintf(stdout, "help: help [builtin_name]\nPrints info about [builtin_name] builtin command\n");
			return 0;
		}

		fprintf(stderr, "help: no help topics match '%s'\n", builtin_name);
		return 1;
	}
	else
	{
		fprintf(stdout, "Shell commands defined internally:\nhelp [builtin_name]\ncd: cd [-L |-P] [directory]\nexport [name] [value]\nunset [name]\n");
		return 0;
	}
}

