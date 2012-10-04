#include "Test.h"

using namespace test;

int main()
{
    TestSuite& testSuite = TestSuite::getInstance();
    // testSuite.setVerbose(true);

    bool allPassed = testSuite.runAllTests();

    return allPassed ? 0 : 1;
}

