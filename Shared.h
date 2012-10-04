#ifndef SHARED_H
#define SHARED_H

#include <assert.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1600) // VS2010
#define ASSERT(X) assert(X); __analysis_assume(X)
#else
#define ASSERT(X) assert(X)
#endif

#endif // SHARED_H
