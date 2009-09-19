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
		usleep(delay*10000);
#endif
		std::cout << i << std::endl;
	}
	protected:
	int i;
};


class Worker : public JobQueue, public ThreadFunction
{
	public:
	Worker(ThreadCondition &completionsignal)
		: JobQueue(), ThreadFunction(), thread(this), completionsignal(completionsignal), cancelled(false), jobcount(0)
	{
		cerr << "Starting worker thread..." << endl;
		thread.Start();
	}
	virtual ~Worker()
	{
		cerr << "Destructing worker thread..." << endl;
		cancelled=true;
		ObtainMutex();
		Broadcast();
		ReleaseMutex();
		thread.WaitFinished();
	}
	virtual int Entry(Thread &t)
	{
		cerr << "Worker thread running..." << endl;
		do
		{
			cerr << "Obtaining mutex" << endl;
			ObtainMutex();
			while(IsEmpty())
			{
				cerr << "Waiting for a job" << endl;
				WaitCondition();
				cerr << "Signal received" << endl;
				if(cancelled)
				{
					cerr << "Received cancellation signal" << endl;
					ReleaseMutex();
					return(0);
				}
			}
			cerr << "Releasing mutex" << endl;
			ReleaseMutex();
			cerr << "Running job" << endl;
			Dispatch();

			completionsignal.ObtainMutex();
			jobcount=JobQueue::JobCount();
			completionsignal.Broadcast();
			completionsignal.ReleaseMutex();

		} while(!cancelled);
		cerr << "Cancelled" << endl;
		return(0);
	}
	virtual void PushJob(Job *job)
	{
		JobQueue::PushJob(job);
		jobcount=JobQueue::JobCount();
	}
	virtual int JobCount()
	{
		return(jobcount);
	}
	protected:
	Thread thread;
	ThreadCondition &completionsignal;
	bool cancelled;
	int jobcount;
};


class JobDispatcher : public ThreadCondition
{
	public:
	JobDispatcher(int threads)
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
	virtual void PushJob(Job *job)
	{
		// FIXME - use a pull model here - add jobs to a central queue and allow the subthreads to "adopt" them?
		cerr << "Adding job to the queues" << endl;
		// Find the thread with the fewest jobs.
		list<Worker *>::iterator it;
		it=threadlist.begin();
		Worker *bestw=NULL;
		int bestcount=9999;
		while(it!=threadlist.end())
		{
			Worker *w=*it;
			int c=w->JobCount();
			if(c<bestcount)
			{
				bestw=w;
				bestcount=c;
			}
			++it;
		}
		if(bestw)
			bestw->PushJob(job);
	}
	virtual int JobsRemaining()
	{
		int result=0;
		list<Worker *>::iterator it=threadlist.begin();
		while(it!=threadlist.end())
		{
			result+=(*it)->JobCount();
			++it;
		}
		return(result);
	}
	virtual void WaitCompletion()
	{
		ObtainMutex();
		list<Worker *>::iterator it=threadlist.begin();
		while(it!=threadlist.end())
		{
			while((*it)->JobCount())
			{
				cerr << "(" << JobsRemaining() << " jobs remaining...)" << endl;
				WaitCondition();
			}
			++it;
		}
		ReleaseMutex();
	}
	protected:
	list<Worker *> threadlist;
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

