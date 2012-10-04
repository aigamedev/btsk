/******************************************************************************
 * This file is part of the Behavior Tree Starter Kit.
 * 
 * Copyright (c) 2012, AiGameDev.com
 * 
 * Credits:         Alex J. Champandard
 *****************************************************************************/

#include <vector>
#include "Shared.h"
#include "Test.h"

namespace bt2
{

// ============================================================================

class Node;
class Task;

enum Status
{
	BH_INVALID,
	BH_SUCCESS,
	BH_FAILURE,
	BH_RUNNING,
};

class Node
{
public:
	virtual Task* create() = 0;
	virtual void destroy(Task*) = 0;

	virtual ~Node() {}
};

class Task
{
public:
	Task(Node& node)
	:	m_pNode(&node)
	{
	}

	virtual ~Task() {}

	virtual Status update() = 0;

	virtual void onInitialize() {}
	virtual void onTerminate(Status) {}

protected:
	Node* m_pNode;
};

class Behavior
{
protected:
	Task* m_pTask;
	Node* m_pNode;
	Status m_eStatus;

public:
	Behavior()
	:	m_pTask(NULL)
	,	m_pNode(NULL)
	,	m_eStatus(BH_INVALID)
	{
	}
	Behavior(Node& node)
	:	m_pTask(NULL)
	,	m_pNode(NULL)
	,	m_eStatus(BH_INVALID)
	{
		setup(node);
	}
	~Behavior()
	{
		m_eStatus = BH_INVALID;
		teardown();
	}

	void setup(Node& node)
	{
		teardown();

		m_pNode = &node;
		m_pTask = node.create();
	}
	void teardown()
	{
		if (m_pTask == NULL)
		{
			return;
		}

		ASSERT(m_eStatus != BH_RUNNING);
		m_pNode->destroy(m_pTask);
		m_pTask = NULL;
	}

	Status tick()
	{
		if (m_eStatus == BH_INVALID)
		{
			m_pTask->onInitialize();
		}

		m_eStatus = m_pTask->update();

		if (m_eStatus != BH_RUNNING)
		{
			m_pTask->onTerminate(m_eStatus);
		}

		return m_eStatus;
	}

	template <class TASK>
	TASK* get() const
	{
		// NOTE: It's safe to static_cast, the caller almost
		// always knows what kind of task type it is.
		return dynamic_cast<TASK*>(m_pTask);
	}
};

// ----------------------------------------------------------------------------

struct MockTask : public Task
{
	int m_iInitializeCalled;
	int m_iTerminateCalled;
	int m_iUpdateCalled;
	Status m_eReturnStatus;
	Status m_eTerminateStatus;

	MockTask(Node& node)
		:	Task(node)
		,	m_iInitializeCalled(0)
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

struct MockNode : public Node
{
	virtual void destroy(Task*) {}
	virtual Task* create()
	{
		m_pTask = new MockTask(*this);
		return m_pTask;
	}

	virtual ~MockNode()
	{
		delete m_pTask;
	}

	MockNode()
	:	m_pTask(NULL)
	{
	}

	MockTask* m_pTask;
};

TEST(StarterKit3, TaskInitialize)
{
	MockNode n;
	Behavior b(n);

	MockTask* t = b.get<MockTask>();
	CHECK_EQUAL(0, t->m_iInitializeCalled);

	b.tick();
	CHECK_EQUAL(1, t->m_iInitializeCalled);
};

TEST(StarterKit3, TaskUpdate)
{
	MockNode n;
	Behavior b(n);

	MockTask* t = b.get<MockTask>();
	CHECK_EQUAL(0, t->m_iUpdateCalled);

	b.tick();
	CHECK_EQUAL(1, t->m_iUpdateCalled);
};

TEST(StarterKit3, TaskTerminate)
{
	MockNode n;
	Behavior b(n);
	b.tick();

	MockTask* t = b.get<MockTask>();
	CHECK_EQUAL(0, t->m_iTerminateCalled);

	t->m_eReturnStatus = BH_SUCCESS;
	b.tick();
	CHECK_EQUAL(1, t->m_iTerminateCalled);
};


// ============================================================================

typedef std::vector<Node*> Nodes;

class Composite : public Node
{
public:	
	Nodes m_Children;
};

class Sequence : public Task
{
public:
	Sequence(Composite& node)
	:	Task(node)
	{
	}

	Composite& getNode()
	{
		return *static_cast<Composite*>(m_pNode);
	}

	virtual void onInitialize()
	{
		m_CurrentChild = getNode().m_Children.begin();
		m_CurrentBehavior.setup(**m_CurrentChild);
	}

	virtual Status update()
	{
		for (;;)
		{
			Status s = m_CurrentBehavior.tick();
			if (s != BH_SUCCESS)
			{
				return s;
			}
			if (++m_CurrentChild == getNode().m_Children.end())
			{
				return BH_SUCCESS;
			}
			m_CurrentBehavior.setup(**m_CurrentChild);
		}
	}

protected:
	Nodes::iterator m_CurrentChild;
	Behavior m_CurrentBehavior;
};

// ----------------------------------------------------------------------------

template <class TASK>
class MockComposite : public Composite
{
public:
	MockComposite(size_t size)
	{
		for (size_t i=0; i<size; ++i)
		{
			m_Children.push_back(new MockNode);
		}
	}

	~MockComposite()
	{
		for (size_t i=0; i<m_Children.size(); ++i)
		{
			delete m_Children[i];
		}
	}

	MockTask& operator[](size_t index)
	{
		ASSERT(index < m_Children.size());
		MockTask* task = static_cast<MockNode*>(m_Children[index])->m_pTask;
		ASSERT(task != NULL);
		return *task;
	}

	virtual Task* create()
	{
		return new TASK(*this);
	}

	virtual void destroy(Task* task)
	{
		delete task;
	}
};

typedef MockComposite<Sequence> MockSequence;

TEST(StarterKit3, SequenceTwoFails)
{
	MockSequence seq(2);
	Behavior bh(seq);

	CHECK_EQUAL(bh.tick(), BH_RUNNING);
	CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

	seq[0].m_eReturnStatus = BH_FAILURE;
	CHECK_EQUAL(bh.tick(), BH_FAILURE);
	CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
}

TEST(StarterKit3, SequenceTwoContinues)
{
	MockSequence seq(2);
	Behavior bh(seq);

	CHECK_EQUAL(bh.tick(), BH_RUNNING);
	CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

	seq[0].m_eReturnStatus = BH_SUCCESS;
	CHECK_EQUAL(bh.tick(), BH_RUNNING);
	CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
}

TEST(StarterKit3, SequenceOnePassThrough)
{
	Status status[2] = { BH_SUCCESS, BH_FAILURE };
	for (int i=0; i<2; ++i)
	{
		MockSequence seq(1);
		Behavior bh(seq);

		CHECK_EQUAL(bh.tick(), BH_RUNNING);
		CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

		seq[0].m_eReturnStatus = status[i];
		CHECK_EQUAL(bh.tick(), status[i]);
		CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
	}
}


// ============================================================================

class Selector : public Task
{
public:
	Selector(Composite& node)
		:	Task(node)
	{
	}

	Composite& getNode()
	{
		return *static_cast<Composite*>(m_pNode);
	}

	virtual void onInitialize()
	{
		m_CurrentChild = getNode().m_Children.begin();
		m_Behavior.setup(**m_CurrentChild);
	}

	virtual Status update()
	{
		for (;;)
		{
			Status s = m_Behavior.tick();
			if (s != BH_FAILURE)
			{
				return s;
			}
			if (++m_CurrentChild == getNode().m_Children.end())
			{
				return BH_FAILURE;
			}
			m_Behavior.setup(**m_CurrentChild);
		}
	}

protected:
	Nodes::iterator m_CurrentChild;
	Behavior m_Behavior;
};

typedef MockComposite<Selector> MockSelector;

TEST(StarterKit3, SelectorOnePassThrough)
{
	Status status[2] = { BH_SUCCESS, BH_FAILURE };
	for (int i=0; i<2; ++i)
	{
		MockSelector sel(1);
		Behavior bh(sel);

		CHECK_EQUAL(bh.tick(), BH_RUNNING);
		CHECK_EQUAL(0, sel[0].m_iTerminateCalled);

		sel[0].m_eReturnStatus = status[i];
		CHECK_EQUAL(bh.tick(), status[i]);
		CHECK_EQUAL(1, sel[0].m_iTerminateCalled);
	}
}

TEST(StarterKit3, SelectorTwoContinues)
{
	MockSelector sel(2);
	Behavior bh(sel);

	CHECK_EQUAL(bh.tick(), BH_RUNNING);
	CHECK_EQUAL(0, sel[0].m_iTerminateCalled);

	sel[0].m_eReturnStatus = BH_FAILURE;
	CHECK_EQUAL(bh.tick(), BH_RUNNING);
	CHECK_EQUAL(1, sel[0].m_iTerminateCalled);
}

TEST(StarterKit3, SelectorTwoSucceeds)
{
	MockSelector sel(2);
	Behavior bh(sel);

	CHECK_EQUAL(bh.tick(), BH_RUNNING);
	CHECK_EQUAL(0, sel[0].m_iTerminateCalled);

	sel[0].m_eReturnStatus = BH_SUCCESS;
	CHECK_EQUAL(bh.tick(), BH_SUCCESS);
	CHECK_EQUAL(1, sel[0].m_iTerminateCalled);
}

} // namespace bt2
