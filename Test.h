#ifndef TEST_H
#define TEST_H

#include <iostream>
#include <sstream>
#include <vector>
#include <functional>

#include "Shared.h"

namespace test
{
    typedef std::function<void()> Test;

    class TestFailedException
    {
    public:
        TestFailedException(std::string message)
        :   m_message(message)
        {
        }

        std::string m_message;
    };

    class TestSuite
    {
    private:
        typedef std::pair<std::string, Test> RegisteredTest;
        typedef std::vector<RegisteredTest> RegisteredTests;

    public:
        TestSuite()
        :   m_verbose(false)
        {
        }

        void registerTest(const std::string& testName, Test test)
        {
            m_tests.push_back(RegisteredTest(testName, test));
        }

        void setVerbose(bool b)
        {
            m_verbose = b;
        }

        bool runAllTests()
        {
            bool allPassed = true;
            for (RegisteredTests::const_iterator test = m_tests.begin(); test != m_tests.end(); ++test)
            {
                if (m_verbose)
                {
                    std::cout << test->first << " " << std::flush;
                }
                try
                {
                    test->second();
                    if (m_verbose)
                    {
                        std::cout << "PASS" << std::endl << std::flush;
                    }
                    else
                    {
                        std::cout << "." << std::flush;
                    }
                }
                catch (TestFailedException e)
                {
                    allPassed = false;
                    if (!m_verbose)
                    {
                        std::cout << test->first << " ";
                    }
                    std::cout << "FAIL: " << e.m_message << std::endl << std::flush;
                }
            }
            if (!m_verbose)
            {
                std::cout << std::endl << std::flush;
            }
            return allPassed;
        }

        static TestSuite& getInstance()
        {
            static TestSuite instance;
            return instance;
        }

    private:
        bool m_verbose;
        RegisteredTests m_tests;
    };

    class AutoTestRegister
    {
    public:
        AutoTestRegister(const std::string& name, Test test)
        {
            TestSuite::getInstance().registerTest(name, test);
        }
    };
}

#define TEST(SUITENAME, TESTNAME)                             \
    void SUITENAME##_##TESTNAME();                            \
    static test::AutoTestRegister autoTestRegister_##SUITENAME##_##TESTNAME(#SUITENAME "_" #TESTNAME, SUITENAME##_##TESTNAME); \
    void SUITENAME##_##TESTNAME()

#define CHECK(X)                                              \
    if (!(X))                                                 \
    {                                                         \
        std::ostringstream os;                                \
        os << #X << " ";                                      \
        os << __FILE__ << ":" << __LINE__;                    \
        throw test::TestFailedException(os.str());            \
    }

#define CHECK_EQUAL(X, Y)                                     \
    if (!((X) == (Y)))                                        \
    {                                                         \
        std::ostringstream os;                                \
        os << "(" << #X << " == " << #Y << ") ";              \
        os << "expected:" << (X) << " actual:" << (Y) << " "; \
        os << __FILE__ << ":" << __LINE__;                    \
        throw test::TestFailedException(os.str());            \
    }

#endif // TEST_H
