/*************************************************
 * C program to count no of lines, words and 	 *
 * characters in a file.		                 *
 *************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_PROC 100
#define MAX_FORK 1000

typedef struct {
	int linecount;
	int wordcount;
	int charcount;
} count_t;

typedef struct {
	int pid;				// Process ID
	long offset;			// Offset for process
	long lengthAssigned;	// Length assigned to process
	int pipefd[2];			// file descriptor for pipe
	bool finished;			// status on whether final status has been processed
	int status;
} plist_t;

int CRASH = 0;

count_t word_count(FILE* fp, long offset, long size)
{
	char ch;
	long rbytes = 0;

	count_t count;
	// Initialize counter variables
	count.linecount = 0;
	count.wordcount = 0;
	count.charcount = 0;

	printf("[pid %d] reading %ld bytes from offset %ld\n", getpid(), size, offset);

	if(fseek(fp, offset, SEEK_SET) < 0) {
		printf("[pid %d] fseek error!\n", getpid());
	}

	while ((ch=getc(fp)) != EOF && rbytes < size) {
		// Increment character count if NOT new line or space
		if (ch != ' ' && ch != '\n') { ++count.charcount; }

		// Increment word count if new line or space character
		if (ch == ' ' || ch == '\n') { ++count.wordcount; }

		// Increment line count if new line character
		if (ch == '\n') { ++count.linecount; }
		rbytes++;
	}

	srand(getpid());
	if(CRASH > 0 && (rand()%100 < CRASH))
	{
		printf("[pid %d] crashed.\n", getpid());
		abort();
	}

	return count;
}

int main(int argc, char **argv)
{
	long fsize;
	FILE *fp;
	int numJobs;
	plist_t plist[MAX_PROC]; // array of processes
	count_t total = {0, 0, 0}, count, result;
	int i, pid, status;
	int nFork = 0;

	if(argc < 3) {
		printf("usage: wc_mul <# of processes> <filname>\n");
		return 0;
	}

	if(argc > 3) {
		CRASH = atoi(argv[3]);
		if(CRASH < 0) CRASH = 0;
		if(CRASH > 50) CRASH = 50;
	}
	printf("CRASH RATE: %d\n", CRASH);

	numJobs = atoi(argv[1]);
	if(numJobs > MAX_PROC) numJobs = MAX_PROC;

	// Open file in read-only mode
	fp = fopen(argv[2], "r");

	if(fp == NULL) {
		printf("File open error: %s\n", argv[2]);
		printf("usage: wc <# of processes> <filname>\n");
		return 0;
	}

	fseek(fp, 0L, SEEK_END);
	fsize = ftell(fp);
	fclose(fp);

	long sizePerProcesss = fsize / numJobs;	// Work load per child
	long remainder = fsize % numJobs; 		// Leftover work to be distributed

	for(i = 0; i < numJobs; i++) {
		plist[i].offset = sizePerProcesss * i; 			// Calculate offset 
		plist[i].lengthAssigned = sizePerProcesss;		// assign length to process
		plist[i].finished = false;						// Flag to check if process has been finished
		
		// Assign the rest of the work (remainder) to last job if needed
		if ((i == numJobs - 1) && remainder) {
			plist[i].lengthAssigned += remainder;
		}

		if(nFork++ > MAX_FORK) return 0;

		// Set pipe 
		if (pipe(plist[i].pipefd) == -1) {
			perror("Error creating pipe.");
        	exit(EXIT_FAILURE);  // Exit with failure status
		}
		pid = fork();
		if(pid < 0) {
			printf("Fork failed.\n");
		} else if(pid == 0) { // Child
			fp = fopen(argv[2], "r");
			count = word_count(fp, plist[i].offset, plist[i].lengthAssigned);

			// send the result to the parent through pipe
			write(plist[i].pipefd[1], &count, sizeof(count_t)); // Write result to pipe
			close(plist[i].pipefd[1]); // Close write end of pipe
			fclose(fp);
			return 0;
		}
		plist[i].pid = pid;
	}

	// Parent
	// wait for all children
	// check their exit status
	// read the result of normalliy terminated child
	// re-crete new child if there is one or more failed child
	// close pipe
	int successfulJobs = 0;
	while (successfulJobs < numJobs) {

		for (i = 0; i < numJobs; i++) {
			waitpid(plist[i].pid, &plist[i].status, 0);

			// If the child exited abnormally, launch a new child process
			if (WIFSIGNALED(plist[i].status)) {

				// Launch new process to replace this
				pid = fork();
				if (pid < 0) printf("Fork failed.\n");
				else if (pid == 0) {
					fp = fopen(argv[2], "r");
					count = word_count(fp, plist[i].offset, plist[i].lengthAssigned);

					write(plist[i].pipefd[1], &count, sizeof(count_t));	// Write result to pipe
					close(plist[i].pipefd[1]);							// Close write end of pipe
					fclose(fp);
					return 0;
				}
				plist[i].pid = pid;
				
			} else {
				if (!plist[i].finished) {
					read(plist[i].pipefd[0], &result, sizeof(count_t));	// Parent reads result written by child
					close(plist[i].pipefd[0]);							// Close read end of pipe

					// Add results to total
					total.linecount += result.linecount;
					total.wordcount += result.wordcount;
					total.charcount += result.charcount;
					plist[i].finished = true;
					successfulJobs++;
				}
			}
		}
	}

	printf("\n========== Final Results ================\n");
	printf("Total Lines : %d \n", total.linecount);
	printf("Total Words : %d \n", total.wordcount);
	printf("Total Characters : %d \n", total.charcount);
	printf("=========================================\n");

	return(0);
}

