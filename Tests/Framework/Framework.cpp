// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <Tests/Framework/Framework.h>
#include <AnKi/Util/Filesystem.h>
#include <iostream>
#include <cstring>
#include <malloc.h>
#if ANKI_OS_ANDROID
#	include <android/log.h>
#endif

namespace anki {

#if !ANKI_OS_ANDROID
#	define ANKI_TEST_LOG(fmt, ...) printf(fmt "\n", __VA_ARGS__)
#else
#	define ANKI_TEST_LOG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "AnKi Tests", fmt, __VA_ARGS__)
#endif

TestSuite::~TestSuite()
{
	for(Test* t : tests)
	{
		delete t;
	}
}

void Test::run()
{
	ANKI_TEST_LOG("========\nRunning %s %s\n========", suite->name.c_str(), name.c_str());

#if ANKI_COMPILER_GCC_COMPATIBLE
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#if ANKI_OS_LINUX
	struct mallinfo a = mallinfo();
#endif

	callback(*this);

#if ANKI_OS_LINUX
	struct mallinfo b = mallinfo();

	int diff = b.uordblks - a.uordblks;
	if(diff > 0)
	{
		ANKI_TEST_LOG("Test leaks memory: %d", diff);
	}
#endif

#if ANKI_COMPILER_GCC_COMPATIBLE
#	pragma GCC diagnostic pop
#endif
}

void Tester::addTest(const char* name, const char* suiteName, TestCallback callback)
{
	std::vector<TestSuite*>::iterator it;
	for(it = suites.begin(); it != suites.end(); it++)
	{
		if((*it)->name == suiteName)
		{
			break;
		}
	}

	// Not found
	TestSuite* suite = nullptr;
	if(it == suites.end())
	{
		suite = new TestSuite;
		suite->name = suiteName;
		suites.push_back(suite);
	}
	else
	{
		suite = *it;
	}

	// Sanity check
	std::vector<Test*>::iterator it1;
	for(it1 = suite->tests.begin(); it1 != suite->tests.end(); it1++)
	{
		if((*it)->name == name)
		{
			ANKI_TEST_LOG("Test already exists: %s", name);
			return;
		}
	}

	// Add the test
	Test* test = new Test;
	suite->tests.push_back(test);
	test->name = name;
	test->suite = suite;
	test->callback = callback;
}

int Tester::run(int argc, char** argv)
{
	// Parse args
	//
	programName = argv[0];

	std::string helpMessage = "Usage: " + programName + R"( [options]
Options:
  --help         Print this message
  --list-tests   List all the tests
  --suite <name> Run tests only from this suite
  --test <name>  Run this test. --suite needs to be specified)";

	std::string suiteName;
	std::string testName;

	for(int i = 1; i < argc; i++)
	{
		const char* arg = argv[i];
		if(strcmp(arg, "--list-tests") == 0)
		{
			listTests();
			return 0;
		}
		else if(strcmp(arg, "--help") == 0)
		{
			ANKI_TEST_LOG("%s", helpMessage.c_str());
			return 0;
		}
		else if(strcmp(arg, "--suite") == 0)
		{
			++i;
			if(i >= argc)
			{
				ANKI_TEST_LOG("%s", "<name> is missing after --suite");
				return 1;
			}
			suiteName = argv[i];
		}
		else if(strcmp(arg, "--test") == 0)
		{
			++i;
			if(i >= argc)
			{
				ANKI_TEST_LOG("%s", "<name> is missing after --test");
				return 1;
			}
			testName = argv[i];
		}
	}

	// Sanity check
	if(testName.length() > 0 && suiteName.length() == 0)
	{
		ANKI_TEST_LOG("%s", "Specify --suite as well");
		return 1;
	}

	// Run tests
	//
	int passed = 0;
	int run = 0;
	if(argc == 1)
	{
		// Run all
		for(TestSuite* suite : suites)
		{
			for(Test* test : suite->tests)
			{
				++run;
				test->run();
				++passed;
			}
		}
	}
	else
	{
		for(TestSuite* suite : suites)
		{
			if(suite->name == suiteName)
			{
				for(Test* test : suite->tests)
				{
					if(test->name == testName || testName.length() == 0)
					{
						++run;
						test->run();
						++passed;
					}
				}
			}
		}
	}

	int failed = run - passed;
	ANKI_TEST_LOG("========\nRun %d tests, failed %d", run, failed);

	if(failed == 0)
	{
		ANKI_TEST_LOG("%s", "SUCCESS!");
	}
	else
	{
		ANKI_TEST_LOG("%s", "FAILURE");
	}

	return run - passed;
}

int Tester::listTests()
{
	for(TestSuite* suite : suites)
	{
		for(Test* test : suite->tests)
		{
			ANKI_TEST_LOG("%s --suite %s --test %s", programName.c_str(), suite->name.c_str(), test->name.c_str());
		}
	}

	return 0;
}

static Tester* g_testerInstance = nullptr;

Tester& getTesterSingleton()
{
	return *(g_testerInstance ? g_testerInstance : (g_testerInstance = new Tester));
}

void deleteTesterSingleton()
{
	delete g_testerInstance;
}

void initWindow()
{
	NativeWindowInitInfo inf;
	inf.m_width = g_windowWidthCVar;
	inf.m_height = g_windowHeightCVar;
	inf.m_title = "AnKi unit tests";
	NativeWindow::allocateSingleton();
	ANKI_TEST_EXPECT_NO_ERR(NativeWindow::getSingleton().init(inf));
}

void initGrManager()
{
	GrManagerInitInfo inf;
	inf.m_allocCallback = allocAligned;
	String home;
	ANKI_TEST_EXPECT_NO_ERR(getTempDirectory(home));
	inf.m_cacheDirectory = home;
	GrManager::allocateSingleton();
	ANKI_TEST_EXPECT_NO_ERR(GrManager::getSingleton().init(inf));
}

} // end namespace anki
