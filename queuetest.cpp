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
		delete this;
	}
	protected:
	int i;
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

