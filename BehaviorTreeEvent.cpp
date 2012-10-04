/******************************************************************************
 * This file is part of the Behavior Tree Starter Kit.
 * 
 * Copyright (c) 2012, AiGameDev.com
 * 
 * Credits:         Alex J. Champandard
 *****************************************************************************/

#include <functional>
#include <vector>
#include <deque>
#include "Shared.h"
#include "Test.h"

namespace bt4
{

// ============================================================================

enum Status
{
    BH_INVALID,
    BH_SUCCESS,
    BH_FAILURE,
    BH_RUNNING,
    BH_SUSPENDED,
};

typedef std::function<void (Status)> BehaviorObserver;

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

    virtual Status update() = 0;

    virtual void onInitialize() {}
    virtual void onTerminate(Status) {}

    Status m_eStatus;
    BehaviorObserver m_Observer;
};

class BehaviorTree
{
public:

    void start(Behavior& bh, BehaviorObserver* observer = NULL)
    {
        if (observer != NULL)
        {
            bh.m_Observer = *observer;
        }
        m_Behaviors.push_front(&bh);
    }

    void stop(Behavior& bh, Status result)
    {
        ASSERT(result != BH_RUNNING);
        bh.m_eStatus = result;

        if (bh.m_Observer)
        {
            bh.m_Observer(result);
        }
    }

    void tick()
    {
        // Insert an end-of-update marker into the list of tasks.
        m_Behaviors.push_back(NULL);

        // Keep going updating tasks until we encounter the marker.
        while (step())
        {
            continue;
        }
    }

    bool step()
    {
        Behavior* current = m_Behaviors.front();
        m_Behaviors.pop_front();

        // If this is the end-of-update marker, stop processing.
        if (current == NULL)
        {
            return false;
        }

        // Perform the update on this individual task.
        current->tick();

        // Process the observer if the task terminated.
        if (current->m_eStatus != BH_RUNNING  &&  current->m_Observer)
        {
            current->m_Observer(current->m_eStatus);
        }
        else // Otherwise drop it into the queue for the next tick().
        {
            m_Behaviors.push_back(current);
        }
        return true;
    }

protected:
    std::deque<Behavior*> m_Behaviors;
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

TEST(StarterKit4, TaskInitialize)
{
    MockBehavior t;
    BehaviorTree bt;

    bt.start(t);
    CHECK_EQUAL(0, t.m_iInitializeCalled);

    bt.tick();
    CHECK_EQUAL(1, t.m_iInitializeCalled);
};

TEST(StarterKit4, TaskUpdate)
{
    MockBehavior t;
    BehaviorTree bt;

    bt.start(t);
    bt.tick();
    CHECK_EQUAL(1, t.m_iUpdateCalled);

    t.m_eReturnStatus = BH_SUCCESS;
    bt.tick();
    CHECK_EQUAL(2, t.m_iUpdateCalled);
};

TEST(StarterKit4, TaskTerminate)
{
    MockBehavior t;
    BehaviorTree bt;

    bt.start(t);
    bt.tick();
    CHECK_EQUAL(0, t.m_iTerminateCalled);

    t.m_eReturnStatus = BH_SUCCESS;
    bt.tick();
    CHECK_EQUAL(1, t.m_iTerminateCalled);
};


// ============================================================================

class Composite : public Behavior
{
public:
    typedef std::vector<Behavior*> Behaviors;
    Behaviors m_Children;
};

class Sequence : public Composite
{
public:
    Sequence(BehaviorTree& bt)
    :	m_pBehaviorTree(&bt)
    {
    }

protected:
    BehaviorTree* m_pBehaviorTree;

    virtual void onInitialize()
    {
        m_Current = m_Children.begin();
        BehaviorObserver observer = std::bind(&Sequence::onChildComplete, this, std::placeholders::_1);
        m_pBehaviorTree->start(**m_Current, &observer);
    }

    void onChildComplete(Status)
    {
        Behavior& child = **m_Current;
        if (child.m_eStatus == BH_FAILURE)
        {
            m_pBehaviorTree->stop(*this, BH_FAILURE);
            return;
        }

        ASSERT(child.m_eStatus == BH_SUCCESS);
        if (++m_Current == m_Children.end())
        {
            m_pBehaviorTree->stop(*this, BH_SUCCESS);
        }
        else
        {
            BehaviorObserver observer = std::bind(&Sequence::onChildComplete, this, std::placeholders::_1);
            m_pBehaviorTree->start(**m_Current, &observer);
        }
    }

    virtual Status update()
    {
        return BH_RUNNING;
    }

    Behaviors::iterator m_Current;
};

// ----------------------------------------------------------------------------

template <class COMPOSITE>
class MockComposite : public COMPOSITE
{
public:
    MockComposite(BehaviorTree& bt, size_t size)
    :	COMPOSITE(bt)	
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

TEST(StarterKit4, SequenceOnePassThrough)
{
    Status status[2] = { BH_SUCCESS, BH_FAILURE };
    for (int i=0; i<2; ++i)
    {
        BehaviorTree bt;
        MockSequence seq(bt, 1);

        bt.start(seq);
        bt.tick();
        CHECK_EQUAL(seq.m_eStatus, BH_RUNNING);
        CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

        seq[0].m_eReturnStatus = status[i];
        bt.tick();
        CHECK_EQUAL(seq.m_eStatus, status[i]);
        CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
    }
}

TEST(StarterKit4, SequenceTwoFails)
{
    BehaviorTree bt;
    MockSequence seq(bt, 2);

    bt.start(seq);
    bt.tick();
    CHECK_EQUAL(seq.m_eStatus, BH_RUNNING);
    CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

    seq[0].m_eReturnStatus = BH_FAILURE;
    bt.tick();
    CHECK_EQUAL(seq.m_eStatus, BH_FAILURE);
    CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
}

TEST(StarterKit4, SequenceTwoContinues)
{
    BehaviorTree bt;
    MockSequence seq(bt, 2);

    bt.start(seq);
    bt.tick();
    CHECK_EQUAL(seq.m_eStatus, BH_RUNNING);
    CHECK_EQUAL(0, seq[0].m_iTerminateCalled);

    seq[0].m_eReturnStatus = BH_SUCCESS;
    bt.tick();
    CHECK_EQUAL(seq.m_eStatus, BH_RUNNING);
    CHECK_EQUAL(1, seq[0].m_iTerminateCalled);
}

} // namespace bt4
