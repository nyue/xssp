// Copyright Maarten L. Hekkelman, Radboud University 2008-2011.
//   Distributed under the Boost Software License, Version 1.0.
//       (See accompanying file LICENSE_1_0.txt or copy at    
//             http://www.boost.org/LICENSE_1_0.txt)      

#include "mas.h"

#include <iostream>
#if defined(_MSC_VER)
#define TERM_WIDTH 80
#else
#include <termios.h>
#include <sys/ioctl.h>
#endif

#include <cstdio>

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH
//#include <boost/timer/timer.hpp>
#include <boost/lexical_cast.hpp>

#include "progress.h"

using namespace std;

#ifdef _MSC_VER

#include <Windows.h>

uint32 get_terminal_width()
{
	return TERM_WIDTH;
}

int64 MCounter::operator++(int)
{
	return ::InterlockedExchangeAdd64(&m_value, 1);
} 

int64 MCounter::operator+=(int64 inValue)
{
	return ::InterlockedExchangeAdd64(&m_value, inValue);
}

int64 MCounter::operator=(int64 inValue)
{
	::InterlockedExchange64(&m_value, inValue);
	return inValue;
}

// --------------------------------------------------------------------

#define STDOUT_FILENO 1
bool isatty(int) { return true; }

#else

uint32 get_terminal_width()
{
	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	return w.ws_col;
}

#endif

struct MProgressImpl
{
					MProgressImpl(int64 inMax, const string& inAction)
						: mMax(inMax), mConsumed(0), mAction(inAction), mMessage(inAction)
						, mThread(boost::bind(&MProgressImpl::Run, this)) {}

	void			Run();
	
	void			PrintProgress();
	void			PrintDone();

	int64			mMax;
	MCounter		mConsumed;
	string			mAction, mMessage;
	boost::mutex	mMutex;
	boost::thread	mThread;
//	boost::timer::cpu_timer
//					mTimer;
};

void MProgressImpl::Run()
{
	try
	{
		for (;;)
		{
			boost::this_thread::sleep(boost::posix_time::seconds(1));
			
			boost::mutex::scoped_lock lock(mMutex);
			
			if (mConsumed == mMax)
				break;
			
			PrintProgress();
		}
	}
	catch (...) {}
	
	PrintDone();
}

void MProgressImpl::PrintProgress()
{
	int width = get_terminal_width();
	
	string msg;
	msg.reserve(width + 1);
	if (mMessage.length() <= 20)
	{
		msg = mMessage;
		if (msg.length() < 20)
			msg.append(20 - msg.length(), ' ');
	}
	else
		msg = mMessage.substr(0, 17) + "...";
	
	msg += " [";
	
	float progress = static_cast<float>(mConsumed) / mMax;
	int tw = width - 28;
	int twd = static_cast<int>(tw * progress + 0.5f);
	msg.append(twd, '=');
	msg.append(tw - twd, ' ');
	msg.append("] ");
	
	int perc = static_cast<int>(100 * progress);
	if (perc < 100)
		msg += ' ';
	if (perc < 10)
		msg += ' ';
	msg += boost::lexical_cast<string>(perc);
	msg += '%';
	
	cout << '\r' << msg;
	cout.flush();
}

void MProgressImpl::PrintDone()
{
	string msg;
//	string msg = mAction + " done in " + mTimer.format(0, "%ts cpu / %ws wall");

	int width = get_terminal_width();

	if (msg.length() < width)
		msg += string(width - msg.length(), ' ');
	
	cout << '\r' << msg << endl;
}

MProgress::MProgress(int64 inMax, const string& inAction)
	: mImpl(nullptr)
{
	if (isatty(STDOUT_FILENO))
		mImpl = new MProgressImpl(inMax, inAction);
}

MProgress::~MProgress()
{
	if (mImpl != nullptr and mImpl->mThread.joinable())
	{
		mImpl->mThread.interrupt();
		mImpl->mThread.join();
	}

	delete mImpl;
}
	
void MProgress::Consumed(int64 inConsumed)
{
	if (mImpl != nullptr and 
		(mImpl->mConsumed += inConsumed) >= mImpl->mMax and
		mImpl->mThread.joinable())
	{
		mImpl->mThread.interrupt();
		mImpl->mThread.join();
	}
}

void MProgress::Progress(int64 inProgress)
{
	if (mImpl != nullptr and 
		(mImpl->mConsumed = inProgress) >= mImpl->mMax and
		mImpl->mThread.joinable())
	{
		mImpl->mThread.interrupt();
		mImpl->mThread.join();
	}
}

void MProgress::Message(const std::string& inMessage)
{
	if (mImpl != nullptr)
	{
		boost::mutex::scoped_lock lock(mImpl->mMutex);
		mImpl->mMessage = inMessage;
	}
}
