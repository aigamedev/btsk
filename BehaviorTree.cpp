/******************************************************************************
 * This file is part of the Behavior Tree Starter Kit.
 * 
 * Copyright (c) 2012, AiGameDev.com.
 * 
 * Credits:         Alex J. Champandard
 *****************************************************************************/

#include <vector>
#include "Shared.h"
#include "Test.h"

namespace bt1
{

// ============================================================================

enum Status
/**
 * Return values of and valid states for behaviors.
 */
{
	BH_INVALID,
	BH_SUCCESS,
	BH_FAILURE,
	BH_RUNNING,
};

class Behavior
/**
 * Base class for actions, conditions and composites.
 */
{
protected:
	virtual Status update()				= 0;

	virtual void onInitialize()			{}
	virtual void onTerminate(Status)	{}

public:
	Behavior()
		:	m_eStatus(BH_INVALID)
	{
	}

	Status tick()
	{
		if (m_eStatus == BH_INVALID)
		{
			onInitialize();
		}

		m_eStatus = update();

		if (m_eStatus != BH_RUNNING)
		{
			onTerminate(m_eStatus);
		}
		return m_eStatus;
	}

private:
	Status m_eStatus;
};

// ----------------------------------------------------------------------------

struct MockBehavior : public Behavior
{
	int m_iInitializeCalled;
	int m_iTerminateCalled;
	int m_iUpdateCalled;
	Status m_eReturnStatus;
	Status m_eTerminateStatus;

	MockBehavior()
	:	m_iInitializeCalled(0)
	,	m_iTerminateCalled(0)
	,	m_iUpdateCalled(0)
	,	m_eReturnStatus(BH_RUNNING)
	,	m_eTerminateStatus(BH_INVALID)
	{
	}

	virtual void onInitialize()
	{
		++m_iInitializeCalled;
	}

	virtual void onTerminate(Status s)
	{
		++m_iTerminateCalled;
		m_eTerminateStatus = s;
	}
	
	virtual Status update()
	{
		++m_iUpdateCalled;
		return m_eReturnStatus;
	}
};

TEST(StarterKit1, TaskInitialize)
{
	MockBehavior t;
	CHECK_EQUAL(0, t.m_iInitializeCalled);

	t.tick();
	CHECK_EQUAL(1, t.m_iInitializeCalled);
};

TEST(StarterKit1, TaskUpdate)
{
	MockBehavior t;
	CHECK_EQUAL(0, t.m_iUpdateCalled);

	t.tick();
	CHECK_EQUAL(1, t.m_iUpdateCalled);
};

TEST(StarterKit1, TaskTerminate)
{
	MockBehavior t;
	t.tick();
	CHECK_EQUAL(0, t.m_iTerminateCalled);

	t.m_eReturnStatus = BH_SUCCESS;
	t.tick();
	CHECK_EQUAL(1, t.m_iTerminateCalled);
};


// ============================================================================

class Composite : public Behavior
{
protected:
	typedef std::vector<Behavior*> Behaviors;
	Behaviors m_Children;
};

class Sequence : public Composite
{
protected:
	virtual void onInitialize()
	{
		m_CurrentChild = m_Children.begin();
	}

	virtual Status update()
	{
		// Keep going until a child behavior says its running.
		for (;;)
		{
			Status s = (*m_CurrentChild)->tick();

			// If the child fails, or keeps running, do the same.
			if (s != BH_SUCCESS)
			{
				return s;
			}

			// Hit the end of the array, job done!
			if (++m_CurrentChild == m_Children.end())
			{
				return BH_SUCCESS;
			}
		}
	}

	Behaviors::iterator m_CurrentChild;
};

// ----------------------------------------------------------------------------

template <class COMPOSITE>
class MockComposite : public COMPOSITE
{
public:
	MockComposite(size_t size)
	{
		for (size_t i=0; i<size; ++i)
		{
			COMPOSITE::m_Children.push_back(new MockBehavior);
		}
	}

	~MockComposite()
	{
		for (size_t i=0; i<COMPOSITE::m_Children.size(); ++i)
		{
			delete COMPOSITE::m_Children[i];
		}
	}

	MockBehavior& operator[](size_t index)
	{
		ASSERT(index < COMPOSITE::m_Children.size());
		return *static_cast<MockBehavior*>(COMPOSITE::m_Children[index]);
	}
};

typedef MockComposite<Sequence> MockSequence;

TEST(StarterKit1, SequenceTwoChildrenFails)
{
	MockSequence seq(2);

	CHECK_EQUAL(seq.tick(), BH_RUNNING);
	CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

	seq[0].m_eReturnStatus = BH_FAILURE;
	CHECK_EQUAL(seq.tick(), BH_FAILURE);
	CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
	CHECK_EQUAL(0, seq[1].m_iInitializeCalled);
}

TEST(StarterKit1, SequenceTwoChildrenContinues)
{
	MockSequence seq(2);

	CHECK_EQUAL(seq.tick(), BH_RUNNING);
	CHECK_EQUAL(0, seq[0].m_iTerminateCalled);
	CHECK_EQUAL(0, seq[1].m_iInitializeCalled);

	seq[0].m_eReturnStatus = BH_SUCCESS;
	CHECK_EQUAL(seq.tick(), BH_RUNNING);
	CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
	CHECK_EQUAL(1, seq[1].m_iInitializeCalled);
}

TEST(StarterKit1, SequenceOneChildPassThrough)
{
	Status status[2] = { BH_SUCCESS, BH_FAILURE };
	for (int i=0; i<2; ++i)
	{
		MockSequence seq(1);

		CHECK_EQUAL(seq.tick(), BH_RUNNING);
		CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

		seq[0].m_eReturnStatus = status[i];
		CHECK_EQUAL(seq.tick(), status[i]);
		CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
	}
}


// ============================================================================

class Selector : public Composite
{
protected:
	virtual void onInitialize()
	{
		m_Current = m_Children.begin();
	}

	virtual Status update()
	{		
		// Keep going until a child behavior says its running.
		for (;;)
		{
			Status s = (*m_Current)->tick();

			// If the child succeeds, or keeps running, do the same.
			if (s != BH_FAILURE)
			{
				return s;
			}

			// Hit the end of the array, it didn't end well...
			if (++m_Current == m_Children.end())
			{
				return BH_FAILURE;
			}
		}
	}

	Behaviors::iterator m_Current;
};

// ----------------------------------------------------------------------------

typedef MockComposite<Selector> MockSelector;

TEST(StarterKit1, SelectorTwoChildrenContinues)
{
	MockSelector seq(2);

	CHECK_EQUAL(seq.tick(), BH_RUNNING);
	CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

	seq[0].m_eReturnStatus = BH_FAILURE;
	CHECK_EQUAL(seq.tick(), BH_RUNNING);
	CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
}

TEST(StarterKit1, SelectorTwoChildrenSucceeds)
{
	MockSelector seq(2);

	CHECK_EQUAL(seq.tick(), BH_RUNNING);
	CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

	seq[0].m_eReturnStatus = BH_SUCCESS;
	CHECK_EQUAL(seq.tick(), BH_SUCCESS);
	CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
}

TEST(StarterKit1, SelectorOneChildPassThrough)
{
	Status status[2] = { BH_SUCCESS, BH_FAILURE };
	for (int i=0; i<2; ++i)
	{
		MockSelector seq(1);

		CHECK_EQUAL(seq.tick(), BH_RUNNING);
		CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

		seq[0].m_eReturnStatus = status[i];
		CHECK_EQUAL(seq.tick(), status[i]);
		CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
	}
}

} // namespace bt1
