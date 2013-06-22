#ifndef f_TEST_H
#define f_TEST_H

#include <vd2/system/error.h>

typedef int (*TestFn)();

extern void AddTest(TestFn, const char *);

#define DEFINE_TEST(name) int Test##name(); namespace { struct TestAutoInit_##name { TestAutoInit_##name() { AddTest(Test##name, #name); } } g_testAutoInit_##name; } int Test##name()

typedef MyError AssertionException;

#define TEST_ASSERT(condition) if ((condition)); else throw AssertionException("Test assertion failed: " #condition)

#endif
