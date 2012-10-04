#include <iostream>
#include "Test.h"

using namespace test;

int main()
{
    TestSuite& testSuite = TestSuite::getInstance();
    // testSuite.setVerbose(true);

    bool allPassed = testSuite.runAllTests();
	std::cerr << (allPassed ? "Success." : "Failure!") << std::endl;
    return allPassed ? 0 : 1;
}

