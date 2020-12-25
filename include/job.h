#ifndef JOB_H
#define JOB_H

#include "list.h"
#include <sys/types.h>

struct Process
{
	char** argv;
	pid_t pid;
	char completed;
	char stopped;
	int status;
	int rc;
};

typedef struct List Processes; // list contains Process structures

struct Job
{
	pid_t pgid;
	Processes* processes; 
	char notified;
};

typedef struct List Jobs; // list contains Job structures

struct Job* find_job(Jobs* jobs, pid_t pgid);
int is_job_stopped(struct Job* job);
int is_job_completed(struct Job* job);
void destroy_job(void* job);
void destroy_process(void* process);
void wait_for_job(Jobs* jobs, struct Job* job);
void do_job_notification(Jobs* jobs);

void mark_job_as_running(struct Job* job);
void continue_job(pid_t init_pgid, Jobs* jobs, struct Job* job, int foreground);
void put_job_in_foreground(pid_t init_pgid, Jobs* jobs, struct Job* job);
void put_job_in_background(struct Job* job);

void free_cmd_args(char** argv);

#endif