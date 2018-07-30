#include "types.h"
#include "user.h"
#include "fcntl.h"

#define FORK_NUM			40

#define SIG_DFL -1
#define SIG_IGN 1

#define SIGKILL 9
#define SIGSTOP 17
#define SIGCONT 19

#define HANDLER1 112
#define HANDLER2 114
#define HANDLER3 115

int fail=0;

void foo(int x)
{	
	fail=1;
}
int fib(int n)
{
	if(n==0 ||n==1)
		{return n;}
	else return fib(n-1)+fib(n-2);
}

void user_func1(int n)
 {
	printf(1,"Enter user_func %d\n",n);
	exit();
 }

 void user_func2(int n)
 {
	printf(1,"Enter user_func %d\n",n);
	exit();
 }
 
 void user_func3(int n)
 {
	printf(1,"Enter user_func %d\n",n);
	exit();
 }



void test1()
{
	printf(1,"Test1\n");
	int i;
	for(i=0; i<FORK_NUM; i++)
	{
		int pid = fork();
		if(pid<0)
		{
			printf(1,"fork failed\n");
			exit();
		}
		if(pid == 0)
		{
			if(i % 3 == 0)
			{
				signal(2,&user_func1);
				kill(getpid(), 2);
			}
			if(i % 3 == 1)
			{
				signal(3,&user_func2);
				kill(getpid(), 3);
			}
			if(i % 3 == 2)
			{
				signal(4,&user_func3);
				kill(getpid(), 4);
			}
		}
		else
		{
			wait();
		}

	}

	
	printf(1,"Test1 Passed\n\n");

}

void test2()
{
	printf(1,"Test2\n");

	int pid = fork();

	if(pid ==0)
	{
		int sum=0;
		int t=0;
		for(;t<1000000;t++)
		{
			sum=(sum+t)%500;
		}
		int u=0;
		for(;u<10;u++)
		{
			fib(25);
			}		
		printf(1,"child done\n");
		exit();
	}

	if(pid<0)
	{
		printf(1,"fork failed\n");
		exit();
	}

	else
	{

		kill(pid,SIGSTOP);
		int sum=9;
		int i=0;
		for(;i<100;i++)
		{
			sum+=i;
		}
		printf(1,"%d\n",fib(35));
		printf(1,"sum= %d\n",sum);
		int u=0;
		for(;u<1000;u++)
		{
		}
		sleep(30);
		kill(pid,SIGCONT);
		wait();
	}


	printf(1,"Test2 Passed\n\n");
}


void test5(){
	printf(1,"Test5 \n");
    int father_pid = getpid();
    int p = fork();
    if (p == 0) {
        signal(SIGKILL,(sighandler_t)(SIG_IGN));//1 - child ignores SIGKILL
        sleep(40);
        signal(SIGKILL,(sighandler_t)(SIGKILL)); //3 - child restores SIGKILL handler to default (process should die)
        sleep(50);//child handles SIGKILL after this line
        sleep(500);
        printf(1,"Test5 failed Ignoring SIGKILL!\n");
        kill(father_pid,SIGKILL);
        exit();
    }
    else {
        sleep(5);
        kill(p,SIGKILL);//2 - father tries to send SIGKILL to child, should be ignored
        sleep(60);
        kill(p,SIGKILL);//4 - father tries to send SIGKILL to child, should be executed
        wait();
    }
    printf(1,"Test5 Passed\n\n");
}


void test6()
{
    printf(1,"Test6\n");

    sigprocmask(((1 << 10) | (1 << 15) | (1 << 25)));
    //Ignoring signals 10,15,20,25
    signal(10, (sighandler_t)SIG_IGN);
    signal(15, (sighandler_t)SIG_IGN);
    signal(25, (sighandler_t)SIG_IGN);

    //Changing signal handler of signals 11,16,21,26
    signal(11, (sighandler_t)HANDLER1);
    signal(16, (sighandler_t)HANDLER2);
    signal(26, (sighandler_t)HANDLER3);

    int pid = fork();

    if(pid < 0)
    {
    	printf(1,"Test6 failed in fork\n");
    }
    if (pid == 0) {
        if (sigprocmask(0) != ((1 << 10) | (1 << 15) | (1 << 25))){
            printf(1,"Test6 failed : Child didnt inherit mask array\n");
            exit();
        }
        if (((int)(signal(11,(sighandler_t)SIG_DFL)) != HANDLER1) ||
            ((int)(signal(16,(sighandler_t)SIG_DFL)) != HANDLER2) ||
            ((int)(signal(26,(sighandler_t)SIG_DFL)) != HANDLER3))
        {
            printf(1,"Test6 failed : Child didnt inherit signal handlers\n");
            exit();
        }
        exit();
    }
    else {
        if (sigprocmask(0) != ((1 << 10) | (1 << 15) | (1 << 25))){
            printf(1,"Test6 failed : Parent didnt keep mask array\n");
            exit();
        }
        if (((int)(signal(11,(sighandler_t)SIG_DFL)) != HANDLER1) ||
            ((int)(signal(16,(sighandler_t)SIG_DFL)) != HANDLER2) ||
            ((int)(signal(26,(sighandler_t)SIG_DFL)) != HANDLER3)){
            printf(1,"Test6 failed : Parent didnt keep signal handlers\n");
            exit();
        }
        wait();
    }

    printf(1,"Test6 Passed\n\n");

}


void test7()
{
	printf(1,"Test7\n");
    int parent = getpid();
    int pid = fork();

    if(pid<0)
	{
		printf(1,"fork failed\n");
		exit();
	}

    if (pid ==0){
        sleep(10);
        printf(1,"Test7 failed: child process not stopped!\n");
        kill(parent,SIGKILL);
        exit();
    }
    else {
        kill(pid, SIGSTOP);
        sleep(10);
        kill(pid, SIGKILL);
        kill(pid, SIGCONT);
        wait();
    }
    printf(1,"Test7 Passed\n\n");
}


int main()
{	
	
	
	 test1(); //check user-signals-handlers
	 test2(); //check stop & cont signals

	 /* ======================================================== */
	 // check sigprocmask syscall
	  printf(1,"Test3 \n");

	  int mask11=1<<11;
	  sigprocmask(mask11);
	  signal(11,&foo);
	  kill(getpid(),11);
	  if(fail==1)
	  {
	  	printf(1,"Test3 Failed-mask doesnt work\n");
	  	exit();
	  }
	  printf(1,"Test3 Passed\n\n");


	 //return to default mask for next tests
	 sigprocmask(0);
	 /* ======================================================== */
	 // check signal syscall
	 printf(1,"Test4\n");

	 int tmp =((int)(signal(5,&user_func1)));
	 if(tmp!=SIG_DFL)
	 {
	 	printf(1,"test 4 fail signal 5 wasnt default ");
	 	exit();
	 }
	 printf(1,"Test4 Passed\n\n");

	 /* ======================================================== */

	 test5(); //check ignore handler signal
	 test6(); //check inherit signals handler and singal_mask

 	 test7(); 
	



	exit();

}


