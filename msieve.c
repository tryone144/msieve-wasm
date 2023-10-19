/**
 * Emscripten interface for msieve.
 * Based on `demo.c` provided with the official source distribution.
 */

#include <msieve.h>
#include <signal.h>

// progress callback
extern void publishFactor(uint factor_type, char *factor_number);

msieve_obj *g_curr_factorization = NULL;

/*--------------------------------------------------------------------*/
void handle_signal(int sig) {
	msieve_obj *obj = g_curr_factorization;

	printf("\nreceived signal %d; shutting down\n", sig);

	if (obj && (obj->flags & MSIEVE_FLAG_SIEVING_IN_PROGRESS))
		obj->flags |= MSIEVE_FLAG_STOP_SIEVING;
	else
		_exit(0);
}

/*--------------------------------------------------------------------*/
void get_random_seeds(uint32 *seed1, uint32 *seed2) {

	uint32 tmp_seed1, tmp_seed2;

	/* In a multithreaded program, every msieve object
	 * should have two unique, non-correlated seeds
	 * chosen for it */

	/* Sample the current time, the high-res timer
	 * (hopefully not correlated to the current time),
	 * and the process ID. Multithreaded applications
	 * should fold in the thread ID too */

	uint64 high_res_time = read_clock();
	tmp_seed1 = ((uint32)(high_res_time >> 32) ^ (uint32)time(NULL)) * (uint32)getpid();
	tmp_seed2 = (uint32)high_res_time;

	/* The final seeds are the result of a multiplicative
	 * hash of the initial seeds */

	(*seed1) = tmp_seed1 * ((uint32)40499 * 65543);
	(*seed2) = tmp_seed2 * ((uint32)40499 * 65543);
}

/*--------------------------------------------------------------------*/
void factor_integer(char *buf, uint32 flags,
		    char *savefile_name,
		    char *logfile_name,
		    char *nfs_fbfile_name,
		    uint32 *seed1, uint32 *seed2,
		    uint32 max_relations,
		    enum cpu_type cpu,
		    uint32 cache_size1,
		    uint32 cache_size2,
		    uint32 num_threads,
		    uint32 which_gpu,
		    const char *nfs_args) {

	char *int_start, *last;
	msieve_obj *obj;
	msieve_factor *factor;

	/* point to the start of the integer or expression;
	 * if the start point indicates no integer is present,
	 * don't try to factor it :) */

	last = strchr(buf, '\n');
	if (last)
		*last = 0;
	int_start = buf;
	while (*int_start && !isdigit(*int_start) && *int_start != '(') {
		int_start++;
	}
	if (*int_start == 0)
		return;

	g_curr_factorization = msieve_obj_new(int_start, flags,
					savefile_name, logfile_name,
					nfs_fbfile_name,
					*seed1, *seed2, max_relations,
					cpu, cache_size1, cache_size2,
					num_threads, which_gpu,
					nfs_args);
	if (g_curr_factorization == NULL) {
		fprintf(stderr, "factoring initialization failed\n");
		return;
	}

	msieve_run(g_curr_factorization);

	if (!(g_curr_factorization->flags & MSIEVE_FLAG_FACTORIZATION_DONE)) {
		fprintf(stderr, "\ncurrent factorization was interrupted\n");
		exit(0);
	}

	/* Print out the factors that were found */
	factor = g_curr_factorization->factors;

	printf("\n");
	printf("%s\n", buf);
	while (factor != NULL) {
		char *factor_type;

		if (factor->factor_type == MSIEVE_PRIME)
			factor_type = "p";
		else if (factor->factor_type == MSIEVE_COMPOSITE)
			factor_type = "c";
		else
			factor_type = "prp";

		publishFactor(factor->factor_type, factor->number);
		printf("%s%d: %s\n", factor_type,
				(int32)strlen(factor->number),
				factor->number);
		factor = factor->next;
	}
	printf("\n");

	/* save the current value of the random seeds, so that
	 * the next factorization will pick up the pseudorandom
	 * sequence where this factorization left off */

	*seed1 = g_curr_factorization->seed1;
	*seed2 = g_curr_factorization->seed2;

	/* free the current factorization struct. The following
	 * avoids a race condition in the signal handler */

	obj = g_curr_factorization;
	g_curr_factorization = NULL;
	if (obj)
		msieve_obj_free(obj);
}

/*--------------------------------------------------------------------*/

/** Main(): factorize by sieving */
int sieve(char *input) {
	char *buf = input;
	uint32 seed1, seed2;
	char *savefile_name = NULL;
	char *logfile_name = NULL;
	char *nfs_fbfile_name = NULL;
	uint32 flags = MSIEVE_FLAG_USE_LOGFILE;
	uint32 max_relations = 0; // CONST
	enum cpu_type cpu;
	uint32 cache_size1;
	uint32 cache_size2;
	uint32 num_threads = 0; // CONST
	uint32 which_gpu = 0; // CONST (CUDA only)
	const char *nfs_args = NULL; // CONST

	if (!(isdigit(buf[0]) || buf[0] == '(' )) {
		fprintf(stderr, "cannot parse input '%s'\n", buf);
		return -1;
	}

	get_cache_sizes(&cache_size1, &cache_size2);
	cpu = get_cpu_type();

	if (signal(SIGINT, handle_signal) == SIG_ERR) {
		fprintf(stderr, "could not install handler on SIGINT\n");
		return -1;
	}
	if (signal(SIGTERM, handle_signal) == SIG_ERR) {
		fprintf(stderr, "could not install handler on SIGTERM\n");
		return -1;
	}

	get_random_seeds(&seed1, &seed2);

	factor_integer(buf, flags, savefile_name,
			logfile_name, nfs_fbfile_name,
			&seed1, &seed2,
			max_relations,
			cpu, cache_size1, cache_size2,
			num_threads, which_gpu,
			nfs_args);

	return 0;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s EXPRESSION\n", argv[0]);
		return -1;
	}

	return sieve(argv[1]);
}
