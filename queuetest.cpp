// queue::push/pop
#include <iostream>
#include <queue>
#include <list>
#include <cmath>

#include "support/jobqueue.h"
#include "support/thread.h"
#include "support/multex.h"
#include "support/util.h"

#include "support/debug.h"

using namespace std;

class TestWorker : public Worker
{
	public:
	TestWorker(JobQueue &queue,const char *name) : Worker(queue), name(name)
	{
	}
	virtual ~TestWorker()
	{
	}
	const char *name;
};


class TestJob : public Job
{
	public:
	TestJob(int i) : Job(), i(i)
	{
	}
	virtual ~TestJob()
	{
	}
	virtual void Run(Worker *w)
	{
		TestWorker *tw=(TestWorker *)w;
		double delay=RandomSeeded(20)+1;
#ifdef WIN32
		Sleep(delay*10);
#else
		usleep(delay*100000);
#endif
		std::cout << i << " from " << tw->name << std::endl;
//		delete this;
	}
	protected:
	int i;
};


int main ()
{
	Debug.SetLevel(TRACE);
	JobDispatcher jd(0);

	jd.AddWorker(new TestWorker(jd,"Thread 1"));
	jd.AddWorker(new TestWorker(jd,"Thread 2"));
	jd.AddWorker(new TestWorker(jd,"Thread 3"));

	jd.AddJob(new TestJob(10));
	jd.AddJob(new TestJob(35));
	jd.AddJob(new TestJob(7));
	jd.AddJob(new TestJob(4));
	jd.AddJob(new TestJob(42));
	jd.AddJob(new TestJob(7));
	jd.AddJob(new TestJob(10));
	jd.AddJob(new TestJob(35));
	jd.AddJob(new TestJob(7));
	jd.AddJob(new TestJob(4));
	jd.AddJob(new TestJob(42));
	jd.AddJob(new TestJob(7));
	jd.AddJob(new TestJob(10));
	jd.AddJob(new TestJob(35));
	jd.AddJob(new TestJob(7));
	jd.AddJob(new TestJob(4));
	jd.AddJob(new TestJob(42));
	jd.AddJob(new TestJob(7));

	jd.WaitCompletion();

	return 0;
}

