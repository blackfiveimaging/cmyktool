// queue::push/pop
#include <iostream>
#include <queue>
#include <list>

#include "support/jobqueue.h"
#include "support/thread.h"
#include "support/multex.h"

using namespace std;

class TestJob : public Job
{
	public:
	TestJob(int i) : Job(), i(i)
	{
	}
	virtual ~TestJob()
	{
	}
	virtual void Run()
	{
#ifdef WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
		std::cout << i << std::endl;
	}
	protected:
	int i;
};


class Worker : public ThreadFunction
{
	public:
	Worker(JobQueue &jobqueue,Multex &multex) : ThreadFunction(), jobqueue(jobqueue), thread(this), multex(multex)
	{
		thread.Start();
	}
	virtual ~Worker()
	{
		thread.WaitFinished();
	}
	virtual int Entry(Thread &t)
	{
		multex.ObtainMultex();
		jobqueue.Dispatch();
		multex.ReleaseMultex();
		return(0);
	}
	protected:
	JobQueue &jobqueue;
	Thread thread;
	Multex &multex;
};


class Dispatcher : public ThreadFunction
{
	public:
	Dispatcher(JobQueue &jobqueue, int threads) : ThreadFunction(), thread(this), jobqueue(jobqueue), multex(threads)
	{
		thread.Start();
	}
	virtual ~Dispatcher()
	{
		thread.WaitFinished();
	}
	virtual int Entry(Thread &t)
	{
		jobqueue.ObtainMutex();
		while(!jobqueue.IsEmpty())
		{
			jobqueue.ReleaseMutex();

			multex.ObtainMultex();
			Worker *worker=new Worker(jobqueue,multex);
			threadlist.push_back(worker);
			multex.ReleaseMultex();

			jobqueue.ObtainMutex();
		}
		jobqueue.ReleaseMutex();
		while(!threadlist.empty())
		{
			Worker *thread=threadlist.front();
			delete thread;
			threadlist.pop_front();
		}
		return(0);
	}
	protected:
	Thread thread;
	JobQueue &jobqueue;
	Multex multex;
	list<Worker *> threadlist;
};


int main ()
{
	JobQueue jq;
	jq.Push(new TestJob(10));
	jq.Push(new TestJob(35));
	jq.Push(new TestJob(7));
	jq.Push(new TestJob(4));
	jq.Push(new TestJob(42));
	jq.Push(new TestJob(7));
	jq.Push(new TestJob(10));
	jq.Push(new TestJob(35));
	jq.Push(new TestJob(7));
	jq.Push(new TestJob(4));
	jq.Push(new TestJob(42));
	jq.Push(new TestJob(7));

	Dispatcher d(jq,2);
	std::cerr << "Sleeping..." << std::endl;

	return 0;
}

