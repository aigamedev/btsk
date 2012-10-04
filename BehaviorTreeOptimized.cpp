/******************************************************************************
 * This file is part of the Behavior Tree Starter Kit.
 * 
 * Copyright (c) 2012, AiGameDev.com
 * 
 * Credits:         Alex J. Champandard
 *****************************************************************************/

#include <stdint.h>
#include <vector>
#include <limits>
#include "Shared.h"
#include "Test.h"

namespace bt3
{

// ============================================================================

enum Status
{
    BH_INVALID,
    BH_SUCCESS,
    BH_FAILURE,
    BH_RUNNING,
};

class Behavior
{
public:
    Behavior()
    :	m_eStatus(BH_INVALID)
    {
    }

    virtual ~Behavior()
    {
    }

    Status tick()
    {
        if (m_eStatus == BH_INVALID)
            onInitialize();

        m_eStatus = update();

        if (m_eStatus != BH_RUNNING)
            onTerminate(m_eStatus);

        return m_eStatus;
    }

protected:
	virtual Status update() = 0;

    virtual void onInitialize() {}
    virtual void onTerminate(Status) {}

    Status m_eStatus;
};

typedef std::vector<Behavior*> Behaviors;

// ----------------------------------------------------------------------------

const size_t k_MaxBehaviorTreeMemory = 8192;

class BehaviorTree
{
public:
    BehaviorTree()
	:	m_pBuffer(new uint8_t[k_MaxBehaviorTreeMemory])
    ,	m_iOffset(0)
    {
    }

    ~BehaviorTree()
    {
        delete [] m_pBuffer;
    }

    template <typename T>
    T& allocate()
    {
		T* node = new ((void*)((uintptr_t)m_pBuffer + m_iOffset)) T;
        m_iOffset += sizeof(T);
		ASSERT(m_iOffset < k_MaxBehaviorTreeMemory);
        return *node;
    }

protected:
	uint8_t* m_pBuffer;
    size_t m_iOffset;
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

    virtual ~MockBehavior()
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

TEST(StarterKit2, TaskInitialize)
{
    BehaviorTree bt;
    MockBehavior& t = bt.allocate<MockBehavior>();
    CHECK_EQUAL(0, t.m_iInitializeCalled);

    t.tick();
    CHECK_EQUAL(1, t.m_iInitializeCalled);
};

TEST(StarterKit2, TaskUpdate)
{
    BehaviorTree bt;
    MockBehavior& t = bt.allocate<MockBehavior>();
    CHECK_EQUAL(0, t.m_iUpdateCalled);

    t.tick();
    CHECK_EQUAL(1, t.m_iUpdateCalled);
};

TEST(StarterKit2, TaskTerminate)
{
    BehaviorTree bt;
    MockBehavior& t = bt.allocate<MockBehavior>();

    t.tick();
    CHECK_EQUAL(0, t.m_iTerminateCalled);

    t.m_eReturnStatus = BH_SUCCESS;
    t.tick();
    CHECK_EQUAL(1, t.m_iTerminateCalled);
};


// ============================================================================

const size_t k_MaxChildrenPerComposite = 7;

class Composite : public Behavior
{
public:
    Composite()
    :	m_ChildCount(0)
    {
    }

    void addChild(Behavior& child)
    {
		ASSERT(m_ChildCount < k_MaxChildrenPerComposite);
		ptrdiff_t p = (uintptr_t)&child - (uintptr_t)this;
		ASSERT(p < std::numeric_limits<uint16_t>::max());
		m_Children[m_ChildCount++] = static_cast<uint16_t>(p);
    }

	Behavior& getChild(size_t index)
    {
		ASSERT(index < m_ChildCount);
		return *(Behavior*)((uintptr_t)this + m_Children[index]);
    }

	size_t getChildCount() const
    {
        return m_ChildCount;
    }

private:
	uint16_t m_Children[k_MaxChildrenPerComposite];
	uint16_t m_ChildCount;
};

class Sequence : public Composite
{
protected:
    virtual ~Sequence()
    {
    }

    virtual void onInitialize()
    {
        m_Current = 0;
    }

    virtual Status update()
    {
		for (;;)
        {
            Status s = getChild(m_Current).tick();
            if (s != BH_SUCCESS)
            {
                return s;
            }
            ASSERT(m_Current < std::numeric_limits<uint16_t>::max() - 1);
            if (++m_Current == getChildCount())
            {
                return BH_SUCCESS;
            }
        }
    }

	uint16_t m_Current;
};

// ----------------------------------------------------------------------------

template <class COMPOSITE>
class MockComposite : public COMPOSITE
{
public:
    void initialize(BehaviorTree& tree, size_t size)
    {
        for (size_t i=0; i<size; ++i)
        {
            MockBehavior& t = tree.allocate<MockBehavior>();
            COMPOSITE::addChild(t);
        }
    }

    MockBehavior& operator[](size_t index)
    {
		ASSERT(index < COMPOSITE::getChildCount());
        return *static_cast<MockBehavior*>(&COMPOSITE::getChild(index));
    }
};

typedef MockComposite<Sequence> MockSequence;

TEST(StarterKit2, SequenceOnePassThrough)
{
    BehaviorTree bt;
    Status status[2] = { BH_SUCCESS, BH_FAILURE };
    for (int i=0; i<2; ++i)
    {
        MockSequence& seq = bt.allocate<MockSequence>();
        seq.initialize(bt, 1);

        CHECK_EQUAL(seq.tick(), BH_RUNNING);
        CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

        seq[0].m_eReturnStatus = status[i];
        CHECK_EQUAL(seq.tick(), status[i]);
        CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
    }
}

TEST(StarterKit2, SequenceTwoFails)
{
    BehaviorTree bt;
    MockSequence& seq = bt.allocate<MockSequence>();
    seq.initialize(bt, 2);

    CHECK_EQUAL(seq.tick(), BH_RUNNING);
    CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

    seq[0].m_eReturnStatus = BH_FAILURE;
    CHECK_EQUAL(seq.tick(), BH_FAILURE);
    CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
}

TEST(StarterKit2, SequenceTwoContinues)
{
    BehaviorTree bt;
    MockSequence& seq = bt.allocate<MockSequence>();
    seq.initialize(bt, 2);

    CHECK_EQUAL(seq.tick(), BH_RUNNING);
    CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

    seq[0].m_eReturnStatus = BH_SUCCESS;
    CHECK_EQUAL(seq.tick(), BH_RUNNING);
    CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
}


// ============================================================================

class Selector : public Composite
{
protected:
        virtual ~Selector()
        {
        }

    virtual void onInitialize()
    {
        m_Current = 0;
    }

    virtual Status update()
    {		
		for (;;)
        {
            Status s = getChild(m_Current).tick();
            if (s != BH_FAILURE)
            {
                return s;
            }
            ASSERT(m_Current < std::numeric_limits<uint16_t>::max() - 1);
            if (++m_Current == getChildCount())
            {
                return BH_FAILURE;
            }
        }
    }

	uint16_t m_Current;
};

typedef MockComposite<Selector> MockSelector;

// ----------------------------------------------------------------------------

TEST(StarterKit2, SelectorOnePassThrough)
{
    BehaviorTree bt;

    Status status[2] = { BH_SUCCESS, BH_FAILURE };
    for (int i=0; i<2; ++i)
    {
        MockSelector& sel = bt.allocate<MockSelector>();
        sel.initialize(bt, 1);

        CHECK_EQUAL(sel.tick(), BH_RUNNING);
        CHECK_EQUAL(0, sel[0].m_iTerminateCalled);

        sel[0].m_eReturnStatus = status[i];
        CHECK_EQUAL(sel.tick(), status[i]);
        CHECK_EQUAL(1, sel[0].m_iTerminateCalled);
    }
}

TEST(StarterKit2, SelectorTwoContinues)
{
    BehaviorTree bt;
    MockSelector& sel = bt.allocate<MockSelector>();
    sel.initialize(bt, 2);

    CHECK_EQUAL(sel.tick(), BH_RUNNING);
    CHECK_EQUAL(0, sel[0].m_iTerminateCalled);

    sel[0].m_eReturnStatus = BH_FAILURE;
    CHECK_EQUAL(sel.tick(), BH_RUNNING);
    CHECK_EQUAL(1, sel[0].m_iTerminateCalled);
}

TEST(StarterKit2, SelectorTwoSucceeds)
{
    BehaviorTree bt;
    MockSelector& sel = bt.allocate<MockSelector>();
    sel.initialize(bt, 2);

    CHECK_EQUAL(sel.tick(), BH_RUNNING);
    CHECK_EQUAL(0, sel[0].m_iTerminateCalled);

    sel[0].m_eReturnStatus = BH_SUCCESS;
    CHECK_EQUAL(sel.tick(), BH_SUCCESS);
    CHECK_EQUAL(1, sel[0].m_iTerminateCalled);
}

} // namespace bt3
