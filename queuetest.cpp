// queue::push/pop
#include <iostream>
#include <queue>
#include <list>
#include <cmath>

#include "support/jobqueue.h"
#include "support/thread.h"
#include "support/multex.h"
#include "support/util.h"

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
		double delay=RandomSeeded(20)+1;
#ifdef WIN32
		Sleep(delay*10);
#else
		usleep(delay*100000);
#endif
		std::cout << i << std::endl;
	}
	protected:
	int i;
};


class Worker : public ThreadFunction, public PTMutex
{
	public:
	Worker(JobQueue &queue)
		: ThreadFunction(), PTMutex(), queue(queue), thread(this), cancelled(false)
	{
		cerr << "Starting worker thread..." << endl;
		thread.Start();
	}
	virtual ~Worker()
	{
		cerr << "Destructing worker thread..." << endl;
		cancelled=true;
		while(!thread.TestFinished())
		{
			ObtainMutex();
			queue.ObtainMutex();
			queue.Broadcast();
			queue.ReleaseMutex();
			ReleaseMutex();
		}
	}
	virtual int Entry(Thread &t)
	{
		cerr << "Worker thread running..." << endl;
		do
		{
			cerr << "Obtaining mutex" << endl;
			queue.ObtainMutex();
			while(queue.IsEmpty())
			{
				cerr << "Waiting for a job" << endl;
				queue.WaitCondition();
				cerr << "Signal received" << endl;
				if(cancelled)
				{
					cerr << "Received cancellation signal" << endl;
					queue.ReleaseMutex();
					return(0);
				}
			}
			cerr << "Releasing mutex" << endl;
			queue.ReleaseMutex();
			cerr << "Running job" << endl;

			// Obtain a per-thread mutex while running the job, so the destructor can avoid a busy-wait.
			ObtainMutex();
			queue.Dispatch();
			ReleaseMutex();

			// Send a pulse to say the thread's adopting a new job
			queue.ObtainMutex();
			queue.Broadcast();
			queue.ReleaseMutex();
		} while(!cancelled);
		cerr << "Cancelled" << endl;
		return(0);
	}
	protected:
	JobQueue &queue;
	Thread thread;
	bool cancelled;
};


class JobDispatcher : public JobQueue
{
	public:
	JobDispatcher(int threads) : JobQueue()
	{
		for(int i=0;i<threads;++i)
			threadlist.push_back(new Worker(*this));
	}
	virtual ~JobDispatcher()
	{
		while(!threadlist.empty())
		{
			Worker *thread=threadlist.front();
			delete thread;
			threadlist.pop_front();
		}
	}
	virtual void WaitCompletion()
	{
		ObtainMutex();
		list<Worker *>::iterator it=threadlist.begin();
		while(it!=threadlist.end())
		{
			while(JobCount())
			{
				cerr << "(" << JobCount() << " jobs remaining...)" << endl;
				WaitCondition();
			}
			++it;
		}
		ReleaseMutex();
	}
	protected:
	list<Worker *> threadlist;
	friend class Worker;
};


int main ()
{
	JobDispatcher jd(3);
	jd.PushJob(new TestJob(10));
	jd.PushJob(new TestJob(35));
	jd.PushJob(new TestJob(7));
	jd.PushJob(new TestJob(4));
	jd.PushJob(new TestJob(42));
	jd.PushJob(new TestJob(7));
	jd.PushJob(new TestJob(10));
	jd.PushJob(new TestJob(35));
	jd.PushJob(new TestJob(7));
	jd.PushJob(new TestJob(4));
	jd.PushJob(new TestJob(42));
	jd.PushJob(new TestJob(7));
	jd.PushJob(new TestJob(10));
	jd.PushJob(new TestJob(35));
	jd.PushJob(new TestJob(7));
	jd.PushJob(new TestJob(4));
	jd.PushJob(new TestJob(42));
	jd.PushJob(new TestJob(7));

	jd.WaitCompletion();

	return 0;
}

