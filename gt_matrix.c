#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include "gt_include.h"


#define ROWS 256
#define COLS ROWS
#define SIZE COLS

#define NUM_CPUS 4
#define NUM_GROUPS NUM_CPUS
#define PER_GROUP_COLS (SIZE/NUM_GROUPS)

#define NUM_THREADS 128
#define PER_THREAD_ROWS (SIZE/NUM_THREADS)

// NEW
int type;
int count=0;
long double run[NUM_THREADS];
long double ex[NUM_THREADS];

/* A[SIZE][SIZE] X B[SIZE][SIZE] = C[SIZE][SIZE]
 * Let T(g, t) be thread 't' in group 'g'. 
 * T(g, t) is responsible for multiplication : 
 * A(rows)[(t-1)*SIZE -> (t*SIZE - 1)] X B(cols)[(g-1)*SIZE -> (g*SIZE - 1)] */

typedef struct matrix
{
	int m[SIZE][SIZE];

	int rows;
	int cols;
	unsigned int reserved[2];
} matrix_t;


typedef struct __uthread_arg
{
	matrix_t *_A, *_B, *_C;
	unsigned int reserved0;

	unsigned int tid;
	unsigned int gid;
	int start_row; /* start_row -> (start_row + PER_THREAD_ROWS) */
	int start_col; /* start_col -> (start_col + PER_GROUP_COLS) */
	

	int credit;
	int sz;

}uthread_arg_t;
	
struct timeval tv1;

static void generate_matrix(matrix_t *mat, int val, int sz)
{

	int i,j;
	mat->rows = sz;
	mat->cols = sz;

		for(i = 0; i < mat->rows;i++)
		for( j = 0; j < mat->cols; j++ )
		{
			mat->m[i][j] = val;
		}
	return;
}

static void print_matrix(matrix_t *mat, int sz)
{
	int i, j;


	for(i=0;i<sz;i++)
	{
		for(j=0;j<sz;j++)
			printf(" %d ",mat->m[i][j]);
		printf("\n");
	}

	return;
}

static void * uthread_mulmat(void *p)
{
	int i, j, k;
	int start_row, end_row;
	int start_col, end_col;
	struct timeval tv2, elapsed_runtime;

#define ptr ((uthread_arg_t *)p)

	i=0; j= 0; k=0;

	start_row = ptr->start_row;
	end_row = (ptr->start_row + PER_THREAD_ROWS);

#ifdef GT_GROUP_SPLIT
	start_col = ptr->start_col;
	end_col = (ptr->start_col + PER_THREAD_ROWS);
#else
	start_col = 0;
	end_col = SIZE;
#endif

#ifdef GT_THREADS
	cpuid = kthread_cpu_map[kthread_apic_id()]->cpuid;
	fprintf(stderr, "\nThread(id:%d, group:%d, cpu:%d) started",ptr->tid, ptr->gid, cpuid);
#else
	fprintf(stderr, "\nThread(id:%d, group:%d) started",ptr->tid, ptr->gid);
#endif

	if(ptr->tid == 12 && count==0)
				{
					count=1;
					Thread_yield();
        }
	for(i = 0; i < ptr->sz; i++)
		for(j = 0; j < ptr->sz; j++)
			for(k = 0; k < ptr->sz; k++)
      {
        
				ptr->_C->m[i][j] += ptr->_A->m[i][k] * ptr->_B->m[k][j];
      }

#ifdef GT_THREADS
	printf("\nThread(id: %d , group: %d , cpu: %d ) finished (TIME : %lu s and %lu us)",
			ptr->tid, ptr->gid, cpuid, (tv2.tv_sec - tv1.tv_sec), (tv2.tv_usec - tv1.tv_usec));
#else
	gettimeofday(&tv2,NULL);


	run[ptr->tid] =  ((tv2.tv_sec-tv1.tv_sec)*1000000)+(tv2.tv_usec-tv1.tv_usec);

	printf("\nThread(id: %d , group: %d ) finished (TIME : %lu s and %lu us)",
			ptr->tid, ptr->gid, tv2.tv_sec-tv1.tv_sec, tv2.tv_usec-tv1.tv_usec);
	

#endif

#undef ptr
	return 0;
}

matrix_t A, B, C;

// CHANGED
static void init_matrices(int sz)
{
	generate_matrix(&A, 1, sz);
	generate_matrix(&B, 1, sz);
	generate_matrix(&C, 0, sz);

	return;
}


uthread_arg_t uargs[NUM_THREADS];
uthread_t utids[NUM_THREADS];

int main(int argc, char* argv[])
{
	uthread_arg_t *uarg;
	int inx;

	
	int weight, matrix_size;

	
	
	
	if(argc == 2)
	{
   type = atoi(argv[1]);
   if(type!=1 || type!=0)
   printf("Enter ./bin/matrix 0 for O(1) scheduler and Enter ./bin/matrix 1 for Credit based scheduler");
		
	}
	else
	{
		
		printf("Invalid command. Enter ./bin/matrix 0 for O(1) scheduler and Enter ./bin/matrix 1 for Credit based scheduler"); 
		exit(0);
	}

	gtthread_app_init();

	gettimeofday(&tv1,NULL);



	if (type == 0)
	{
		init_matrices(SIZE);

		for(inx=0; inx<NUM_THREADS; inx++)
		{
			uarg = &uargs[inx];
			uarg->_A = &A;
			uarg->_B = &B;
			uarg->_C = &C;

			uarg->tid = inx;

			uarg->gid = (inx % NUM_GROUPS);

			uarg->start_row = (inx * PER_THREAD_ROWS);

			
			uarg->sz = SIZE;

	#ifdef GT_GROUP_SPLIT
			/* Wanted to split the columns by groups !!! */
			uarg->start_col = (uarg->gid * PER_GROUP_COLS);
	#endif
			uthread_create(&utids[inx], uthread_mulmat, uarg, uarg->gid, 0);
		}
	}

	else
	{
      for(int mat=32,k=0;mat<=256 && k<4;mat=2*mat,k++)
      {
        init_matrices(mat);
        for(int credit=25,i=0;credit<125 && i<4;credit=credit+25,i++)
        {
          for(int j=0;j<8;j++)
          {
          int id = (32*k) + 8 * i + j;

					uarg = &uargs[id];
					uarg->_A = &A;
					uarg->_B = &B;
					uarg->_C = &C;
					uarg->tid = id;
					uarg->gid = (id % NUM_GROUPS);

			#ifdef GT_GROUP_SPLIT
                    
                    uarg->start_col = (uarg->gid * PER_GROUP_COLS);
			#endif
		     uarg->credit = credit;
                     uarg->start_row = 0;
                     uarg->sz=mat;
                    uthread_create(&utids[id], uthread_mulmat, uarg, uarg->gid,credit);
          }
        }
      }
	}

	gtthread_app_exit();



	if(type == 1) 
  {
		printf( "\nRUN TIME \n");
		

		for (int i = 0; i < 128; i++) 
      printf("tid %d : %Lf \n", i, ex[i]);
      
      printf( " EXECUTION TIME \n");
      
  		for (int i = 0; i < 128; i++) 
        printf("tid %d : %Lf \n", i, run[i]);

		
	
         long double mean_run[16]={0};
         long double mean_ex[16]={0};
         long double sd_ex[16]={0};
         long double sd_run[16]={0};
	for(int mat=32, k=0;mat<=256,k<4;mat=2*mat,k++)
       {
      
        for(int credit=25,i=0;credit<125 && i<4;credit=credit+25,i++)
        {
          for(int j=0;j<8;j++)
          {
                int id = (32*k) + 8 * i + j;
                mean_run[(k*4)+i]+=run[id];
                mean_ex[(k*4)+i]+=ex[id];
             
          }
         
         mean_run[(k*4)+i]/=8;
		     mean_ex[(k*4)+i]/=8;
              
      
         }
       }


      for(int mat=32, k=0;mat<=256,k<4;mat=2*mat,k++)
       {
        
        for(int credit=25,i=0;credit<125 && i<4;credit=credit+25,i++)
        {
          for(int j=0;j<8;j++)
          {
                int id = (32*k) + 8 * i + j;
                
                    
                sd_run[(k*4)+i]+= (abs((float)(run[id]/1000)-(float)(mean_run[(k*4)+i])/1000))*(abs((float)(run[id]/1000)-(float)(mean_run[(k*4)+i])/1000));
		sd_ex[(k*4)+i]+= (abs((float)(ex[id])-(float)(mean_ex[(k*4)+i])))*(abs((float)(ex[id])-(float)(mean_ex[(k*4)+i])));
          }
                 sd_run[(k*4)+i]= sqrt((sd_run[(k*4)+i])/(8));
                 sd_ex[(k*4)+i]= sqrt((sd_ex[(k*4)+i])/(8));
                
              
       }
       }

      
     
        for (int inx = 0; inx < 16; inx++)
        {

            printf("Mean Run Time for group %d is %Lf ms \n", inx,(mean_ex[inx]/1000));
            printf("Mean Execution Time for group %d is %Lf ms \n", inx,(mean_run[inx]/1000));
            printf("Standard Run Time Deviations for group %d is %Lf ms \n",inx,sd_ex[inx]/1000);   
            printf("Standard Execution Time Deviations for group %d is %Lf ms \n",inx,sd_run[inx]);
            printf("\n");
        }
      
	}



	// print_matrix(&C);
	// fprintf(stderr, "********************************");
	return(0);
}
