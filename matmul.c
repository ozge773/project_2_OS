#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "matmul.h"
#include <string.h> /*for strlen in parrallem mult*/


void child_process_core(int i,int pipefd, int crashRate);
void serial_mat_mult();
void parallel_mat_mult(int numProc, int crashRate);
void simulate_crash(int crashRate);

int main(int argc, char **argv) 
{
  int crashRate = 0;
  struct timeval  t_begin, t_end;

  if(argc < 2) {
    printf("usage: %s <filename> [crash rate]\n", argv[0]);
    return 0;
  }
		
  /** 1st arg: file name that contains matrices **/
  /** 2nd (optional) arg: crash rate between 0% and 30%. 
   * Each child process has that much chance to crash.
   **/
  if(argc > 2) {
    crashRate = atoi(argv[2]);
    if(crashRate < 0) crashRate = 0;
    if(crashRate > 30) crashRate = 30; 
    printf("Child processes' crash rate: %d\%\n", crashRate);
  }

  read_matrix(argv[1]);

  /** Matrix multiplication **/
  gettimeofday(&t_begin, NULL);
  serial_mat_mult();
  gettimeofday(&t_end, NULL);
		
  printf("================================================\n");
  printf("Serial multiplication took %f seconds.\n",
	 (double) (t_end.tv_usec - t_begin.tv_usec) / 1000000 +
	 (double) (t_end.tv_sec - t_begin.tv_sec));
		
  if(m < 10 && p < 10) {
    printf("The result from the serial calculation: \n");
    print_matrix(C_serial, m, p);
  }
  printf("================================================\n\n");


  /** Multi-process matrix multiplication **/
  gettimeofday(&t_begin, NULL);
  parallel_mat_mult(m, crashRate);
  gettimeofday(&t_end, NULL);
		
  printf("================================================\n");
  printf("Parallel multiplication took %f seconds.\n",
	 (double) (t_end.tv_usec - t_begin.tv_usec) / 1000000 +
	 (double) (t_end.tv_sec - t_begin.tv_sec));
		
  if(m < 10 && p < 10) {
    printf("The result from the parallel calculation: \n");
    print_matrix(C_parallel, m, p);
  }
  printf("================================================\n");

  /** Compare the results **/
  if(compare_matrices(C_serial, C_parallel, m, p) == -1)
    {
      printf("** Serial and parallel results are NOT matched.**\n");
    } else {
    printf("** Serial and parallel results are matched. **\n");
  }
  printf("================================================\n");
  return 0;
}

void serial_mat_mult()
{
  int i, j, k;

  for(i = 0; i < m; i++){
    for(j = 0; j < p; j++){
      C_serial[i][j] = linear_mult(A[i], B_tran[j], n);
    }
  }
}

void child_process_core(int i,int pipefd, int crashRate)
{
  printf("The child process (pid:%d) created to calculate job #(%d/%d).\n", getpid(), i, m);
  simulate_crash(crashRate);
 
  int j;
 
    
  for(j = 0; j <p ; j++){

    // n x m m x p 
    C_parallel[i][j] =  linear_mult(A[i], B_tran[j], n); 
    printf("\n");    
  }
  if(write(pipefd, C_parallel[i], p*sizeof(int))!= p*sizeof(int)){ 
  
    fprintf(stderr, "Child process [%d] failed writing to the pipe, operation #[%d]\n", getpid(), i);
  
    exit(EXIT_FAILURE);
    }
}  

void parallel_mat_mult(int numProc, int crashRate)
{
  int pid[numProc];//numProc is the total number of child processes in this case n
  int pipefd[numProc][2];
  int i;
  int runningChild = numProc;
  int status; //I want to checkif the status after the wait
  	
  for(i = 0; i < numProc; i++)
  {

      
      pipe(pipefd[i]);//
      pid[i] = fork();

 
      if(pid[i] == 0)
       {
        child_process_core(i, pipefd[i][1], crashRate);
        close(pipefd[i][1]); 
        exit(EXIT_SUCCESS);
      
      }else if (pid[i] < 0)
       {
        printf("pid[i] : %d", pid[i]); //
        printf("Fork failed Child#: %d\n", i);
        pid[i] = fork(); //
      }
    }
  int k;
  for(i=0; i< numProc; i++){

    if(waitpid(pid[i], &status, NULL)==-1){
      perror("waitpid");
    }
    if(WIFEXITED(status)){ //normal termination of the child
      close(pipefd[i][1]);
      if(read(pipefd[i][0],C_parallel[i],p*sizeof(int)) != p*sizeof(int)){
        fprintf(stderr, "parent process [%d] failed to read from pipe, operation #[%d]\n", getppid(), i);
        }
      close(pipefd[i][0]);

      k = i;

    }
     // abnormal termination of the child
    //CRASH RECOVERY 
    do{
      k++; //to keep track which child process terminated abnormally
      printf("child process %d terminated by signal\n", k);
      pipe(pipefd[k][0]);
      pid[k] = fork();
      
      if(pid[k] == 0){
        printf("++++");
        child_process_core(k, pipefd[k][1], crashRate);
        printf("Regeneration child process Id :%d", getpid());//to get the current process id of te new child
        close(pipefd[k][1]);
        exit(EXIT_SUCCESS);
      }
    }while(WIFSIGNALED(status) && !WIFEXITED(status));

  }
}

