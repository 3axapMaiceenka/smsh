#include "job.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

struct Job* find_job(Jobs* jobs, pid_t pgid)
{
	if (jobs)
	{
		for (struct Node* node = jobs->head; node; node = node->next)
		{
			struct Job* job = (struct Job*)node->data;

			if (job->pgid == pgid)
			{
				return job;
			} 
		}
	}

	return NULL;	
}

int is_job_stopped(struct Job* job)
{
	if (job->processes)
	{
		for (struct Node* node = job->processes->head; node; node = node->next)
		{
			struct Process* process = (struct Process*)node->data;

			if (!process->stopped && !process->stopped)
			{
				return 0;
			}
		}
	}

	return 1;
}

int is_job_completed(struct Job* job)
{
	if (job->processes)
	{
		for (struct Node* node = job->processes->head; node; node = node->next)
		{
			struct Process* process = (struct Process*)node->data;

			if (!process->completed)
			{
				return 0;
			}
		}
	}

	return 1;
}

void destroy_job(void* job)
{
	if (job)
	{
		struct Job* j = (struct Job*)job;
		destroy_list(&j->processes);
		free(job);
	}
}

void destroy_process(void* process)
{
	if (process)
	{
		struct Process* proc = (struct Process*)process;
		free_cmd_args(proc->argv);
		free(proc);
	}
}

static int mark_process_status(Jobs* jobs, pid_t pid, int status)
{
	if (!pid)
	{
		return -1;
	}

	for (struct Node* node = jobs->head; node; node = node->next)
	{
		struct Job* job = (struct Job*)node->data;

		for (struct Node* n = job->processes->head; n; n = n->next)
		{
			struct Process* process = (struct Process*)n->data;

			if (process->pid == pid)
			{
				process->status = status;

				if (WIFSTOPPED(status))
				{
					process->stopped = 1;
				}
				else
				{
					process->completed = 1;

					if (WIFSIGNALED(status)) 
					{
						fprintf(stderr, "%d: Terminated by signal %d\n", (int)pid, WTERMSIG(process->status));
						process->rc = 129;
					}
					else
					{
						process->rc = WEXITSTATUS(status);
					}
				}

				return 0;
			}
		}
	}

	return -1;
}

void wait_for_job(Jobs* jobs, struct Job* job)
{
	pid_t pid;
	int status;

	do
	{
		pid = waitpid(-1, &status, WUNTRACED);
	} while (!mark_process_status(jobs, pid, status) && !is_job_completed(job) && !is_job_stopped(job));
}

static void format_job_info(struct Job* job, const char* status)
{
	fprintf (stderr, "%ld: %s\n", (long)job->pgid, status);
}

static void update_status(Jobs* jobs)
{
	int status;
	pid_t pid;

	do
	{
		pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
	} while (!mark_process_status(jobs, pid, status));
}

void do_job_notification(Jobs* jobs)
{
	update_status(jobs);

	for (struct Node* node = jobs->head; node; node = node->next)
	{
		struct Job* job = (struct Job*)node->data;

		if (is_job_completed(job))
		{
			struct Node** prev = node == jobs->head ? &jobs->head : &node->prev;

			format_job_info(job, "completed");
			remove_node(jobs, node->data);

			node = *prev;
		}
		else
		{
			if (!job->notified && is_job_stopped(job))
			{
				format_job_info(job, "stopped");
				job->notified = 1;
			}
		}

		if (!node)
		{
			break;
		}
	}
}

void mark_job_as_running(struct Job* job)
{
	for (struct Node* node = job->processes->head; node; node = node->next)
	{
		struct Process* process = (struct Process*)node->data;
		process->stopped = 0;
	}

	job->notified = 0;
}

void continue_job(pid_t init_pgid, Jobs* jobs, struct Job* job, int foreground)
{
	mark_job_as_running(job);

	if (foreground)
	{
		put_job_in_foreground(init_pgid, jobs, job);
	}
	else
	{
		put_job_in_background(job);
	}
}

void put_job_in_foreground(pid_t init_pgid, Jobs* jobs, struct Job* job)
{
	tcsetpgrp(STDIN_FILENO, job->pgid);
	kill(-job->pgid, SIGCONT);

	wait_for_job(jobs, job);

	tcsetpgrp(STDIN_FILENO, init_pgid);

	if (is_job_completed(job))
	{
		remove_node(jobs, job);
	}
}

void put_job_in_background(struct Job* job)
{
	kill(-job->pgid, SIGCONT);
}

void free_cmd_args(char** argv)
{
	if (argv)
	{
		for (char** ptr = argv; *ptr; ptr++)
		{
			free(*ptr);
		}
	
		free(argv);
	}
}