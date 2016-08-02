#include <bdn/init.h>
#include <bdn/test.h>

#include <bdn/mainThread.h>
#include <bdn/Thread.h>
#include <bdn/StopWatch.h>

using namespace bdn;


void testCallFromMainThread(bool throwException)
{
    StopWatch watch;

    SECTION("mainThread")
    {
        int callCount = 0;

        StopWatch watch;

        std::future<int> result = callFromMainThread( [&callCount, throwException](int x){ callCount++; if(throwException){ throw InvalidArgumentError("hello"); } return x*2; }, 42 );

        // should have been called immediately, since we are currently in the main thread
        REQUIRE( callCount==1 );

        REQUIRE( result.wait_for( std::chrono::milliseconds(0)) == std::future_status::ready  );

        if(throwException)
            REQUIRE_THROWS_AS( result.get(), InvalidArgumentError );
        else
            REQUIRE( result.get()==84 );

        // should not have waited at any point.
        REQUIRE( watch.getMillis()<1000 );
    }
    
#if BDN_HAVE_THREADS

	SECTION("otherThread")
	{
        SECTION("storingFuture")
        {
            Thread::exec(
                         [throwException]()
                         {
                             volatile int   callCount = 0;
                             Thread::Id     threadId;

                             std::future<int> result = callFromMainThread(
                                                                          [&callCount, throwException, &threadId](int x)
                                                                          {
                                                                              // sleep a little to ensure that we have time to check callCount
                                                                              Thread::sleepSeconds(1);
                                                                              threadId = Thread::getCurrentId();
                                                                              callCount++;
                                                                              if(throwException)
                                                                                  throw InvalidArgumentError("hello");
                                                                              return x*2;
                                                                          },
                                                                          42 );


                             // should NOT have been called immediately, since we are in a different thread.
                             // Instead the call should have been deferred to the main thread.
                             REQUIRE( callCount==0 );

                             StopWatch threadWatch;

                             REQUIRE( result.wait_for( std::chrono::milliseconds(5000) ) == std::future_status::ready );

                             REQUIRE( threadWatch.getMillis()>=500 );
                             REQUIRE( threadWatch.getMillis()<=5500 );

                             REQUIRE( callCount==1 );

                             REQUIRE( threadId==Thread::getMainId() );
                             REQUIRE( threadId!=Thread::getCurrentId() );

                             threadWatch.start();

                             if(throwException)
                                 REQUIRE_THROWS_AS(result.get(), InvalidArgumentError);
                             else
                                 REQUIRE( result.get()==84 );

                             // should not have waited
                             REQUIRE( threadWatch.getMillis()<=500 );

                             END_ASYNC_TEST();
                         } );


            // time to start thread should have been less than 1000ms
            REQUIRE( watch.getMillis()<1000 );

            MAKE_ASYNC_TEST(10);
        }

        SECTION("notStoringFuture")
        {
            MAKE_ASYNC_TEST(10);

            Thread::exec(
                         [throwException]()
                         {
                             struct Data : public Base
                             {
                                 volatile int callCount = 0;
                             };

                             P<Data> pData = newObj<Data>();

                             StopWatch threadWatch;

                             callFromMainThread(   [pData, throwException](int x)
                                                {
                                                    pData->callCount++;
                                                    if(throwException)
                                                        throw InvalidArgumentError("hello");
                                                    return x*2;
                                                },
                                                42 );


                             // should NOT have been called immediately, since we are in a different thread.
                             // Instead the call should have been deferred to the main thread.
                             REQUIRE( pData->callCount==0 );

                             // should NOT have waited.
                             REQUIRE( threadWatch.getMillis()<1000 );

                             END_ASYNC_TEST();
                         } );


            // time to start thread should have been less than 1000ms
            REQUIRE( watch.getMillis()<1000 );


            // wait a little
            Thread::sleepMillis(2000);
        }
    }
	    
#endif
}

#if BDN_HAVE_THREADS


class TestCallFromMainThreadOrderingBase : public Base
{
public:

    virtual void scheduleCall(std::function<void()> func)=0;
    virtual bool mainThreadCallsShouldExecuteImmediately()=0;

    void start()
    {
        std::list< std::future<void> > futures;

        P<TestCallFromMainThreadOrderingBase> pThis = this;

        // add a call from the main thread first
        {
            MutexLock lock(_mutex);
            _expectedOrder.push_back(-1);

            scheduleCall([pThis]()
                         {
                             pThis->_actualOrder.push_back(-1);
                         });
        }

        // start 100 threads. Each schedules a call in the main thread.
        _scheduledPending = 100;
        for(int i=0; i<_scheduledPending; i++)
        {
            futures.push_back( Thread::exec(
                    [i, pThis]()
                    {
                        MutexLock lock(pThis->_mutex);
                        pThis->_expectedOrder.push_back(i);

                        pThis->scheduleCall([i, pThis]()
                                           {
                                               pThis->_actualOrder.push_back(i);
                                               pThis->onScheduledDone();
                                           });

                    } ) );
        }

        // also add a call from the main thread
        {
            MutexLock lock(_mutex);

            scheduleCall([pThis]()
                          {
                              pThis->_actualOrder.push_back(9999);
                          });

            if(mainThreadCallsShouldExecuteImmediately())
            {
                // if main thread calls are executed immediately then the -1 call already happened
                // and the 9999 call was immediately executed above. So the 9999 call should
                // be on the second position in the order.
                _expectedOrder.insert( _expectedOrder.begin()+1, 9999);
            }
            else
                _expectedOrder.push_back(9999);
        }

        // wait for all threads to finish (i.e. for all callbacks to be scheduled)
        for( std::future<void>& f: futures)
            f.get();

        MAKE_ASYNC_TEST(10);
    }

    void onScheduledDone()
    {
        _scheduledPending--;
        if(_scheduledPending==0)
            onDone();
    }

    void onDone()
    {
        // now verify that the scheduling order and the call order are the same
        for(size_t i=0; i<_expectedOrder.size(); i++)
        {
            REQUIRE( _actualOrder.size()>i);
            REQUIRE( _expectedOrder[i] == _actualOrder[i] );
        }
        REQUIRE( _actualOrder.size() == _expectedOrder.size());

        END_ASYNC_TEST();
    }

    Mutex               _mutex;
    std::vector<int>	_expectedOrder;
    std::vector<int>	_actualOrder;

    int                 _scheduledPending;
};


class TestCallFromMainThreadOrdering_Sync : public TestCallFromMainThreadOrderingBase
{
public:

    void scheduleCall(std::function<void()> func) override
    {
        callFromMainThread(func);
    }

    bool mainThreadCallsShouldExecuteImmediately()
    {
        return true;
    }
};



void testCallFromMainThreadOrdering()
{
    P<TestCallFromMainThreadOrdering_Sync> pTest = newObj<TestCallFromMainThreadOrdering_Sync>();

    pTest->start();
}

#endif

TEST_CASE("callFromMainThread")
{
    SECTION("noException")
        testCallFromMainThread(false);

    SECTION("exception")
        testCallFromMainThread(true);

#if BDN_HAVE_THREADS
	SECTION("ordering")
		testCallFromMainThreadOrdering();
#endif
}



void testAsyncCallFromMainThread(bool throwException)
{
    StopWatch watch;

    struct Data : public Base
    {
        volatile int callCount = 0;
    };


    SECTION("mainThread")
    {
        P<Data> pData = newObj<Data>();

        StopWatch watch;
        
#if BDN_HAVE_THREADS

        asyncCallFromMainThread( [pData, throwException](int x){ pData->callCount++; if(throwException){ throw InvalidArgumentError("hello"); } return x*2; }, 42 );

        // should NOT have been called immediately, even though we are on the main thread
        REQUIRE( pData->callCount==0 );

        // should not have waited at any point.
        REQUIRE( watch.getMillis()<1000 );


        MAKE_ASYNC_TEST(10);

        // start a check thread that waits until the function was called
        // and ends the test
        Thread::exec([pData]()
                     {
                         Thread::sleepMillis(2000);

                         // should have been called now
                         REQUIRE(pData->callCount==1);

                         END_ASYNC_TEST();
                     } );
        
#else

        asyncCallFromMainThread(    [pData, throwException](int x)
                                    {
                                        pData->callCount++;
                                        
                                        asyncCallFromMainThread( [pData](){ REQUIRE(pData->callCount==1); END_ASYNC_TEST(); } );
                                        
                                        if(throwException)
                                            throw InvalidArgumentError("hello");
                                        
                                        return x*2;
                                    }, 42 );
        
        // should NOT have been called immediately, even though we are on the main thread
        REQUIRE( pData->callCount==0 );
        
        // should not have waited at any point.
        REQUIRE( watch.getMillis()<1000 );
        
        MAKE_ASYNC_TEST(10);
        
#endif

    }

#if BDN_HAVE_THREADS

    SECTION("otherThread")
    {
        MAKE_ASYNC_TEST(10);

        Thread::exec(
                     [throwException]()
                     {
                         P<Data> pData = newObj<Data>();

                         StopWatch threadWatch;

                         asyncCallFromMainThread(   [pData, throwException](int x)
                                                    {
                                                        Thread::sleepMillis(2000);
                                                        pData->callCount++;
                                                        if(throwException)
                                                            throw InvalidArgumentError("hello");
                                                        return x*2;
                                                    }
                                                    ,42 );


                         // should NOT have been called immediately, since we are in a different thread.
                         // Instead the call should have been deferred to the main thread.
                         REQUIRE( pData->callCount==0 );

                         // should NOT have waited.
                         REQUIRE( threadWatch.getMillis()<1000 );

                         Thread::sleepMillis(3000);

                         // NOW the function should have been called
                         REQUIRE( pData->callCount==1 );

                         END_ASYNC_TEST();
                     } );

    }
    
#endif

}



#if BDN_HAVE_THREADS


class TestCallFromMainThreadOrdering_Async : public TestCallFromMainThreadOrderingBase
{
public:

    void scheduleCall(std::function<void()> func) override
    {
        asyncCallFromMainThread(func);
    }

    bool mainThreadCallsShouldExecuteImmediately()
    {
        return false;
    }
};

void testAsyncCallFromMainThreadOrdering()
{
    P<TestCallFromMainThreadOrdering_Async> pTest = newObj<TestCallFromMainThreadOrdering_Async>();

    pTest->start();
}

#endif


TEST_CASE("asyncCallFromMainThread")
{
    SECTION("noException")
        testAsyncCallFromMainThread(false);

    SECTION("exception")
        testAsyncCallFromMainThread(true);

#if BDN_HAVE_THREADS
	SECTION("ordering")
		testAsyncCallFromMainThreadOrdering();
#endif
}


void testWrapCallFromMainThread(bool throwException)
{
    StopWatch watch;

    SECTION("mainThread")
    {
        int callCount = 0;

        StopWatch watch;

        auto wrapped = wrapCallFromMainThread<int>( [&callCount, throwException](int val)
                                                    {
                                                        callCount++;
                                                        if(throwException)
                                                            throw InvalidArgumentError("hello");
                                                        return val*2;
                                                    } );

        // should not have been called yet
        REQUIRE( callCount==0 );

        std::future<int> result = wrapped(42);

        // should have been called immediately, since we are currently in the main thread
        REQUIRE( callCount==1 );

        REQUIRE( result.wait_for( std::chrono::milliseconds(0)) == std::future_status::ready  );

        if(throwException)
            REQUIRE_THROWS_AS( result.get(), InvalidArgumentError );
        else
            REQUIRE( result.get()==84 );

        // should not have waited at any point.
        REQUIRE( watch.getMillis()<1000 );
    }
    
#if BDN_HAVE_THREADS

    SECTION("otherThread")
    {
        SECTION("storingFuture")
        {
            Thread::exec(
                         [throwException]()
                         {
                             volatile int   callCount = 0;
                             Thread::Id     threadId;

                             auto wrapped = wrapCallFromMainThread<int>([&callCount, throwException, &threadId](int x)
                                                                        {
                                                                            // sleep a little to ensure that we have time to check callCount
                                                                            Thread::sleepSeconds(1);
                                                                            threadId = Thread::getCurrentId();
                                                                            callCount++;
                                                                            if(throwException)
                                                                                throw InvalidArgumentError("hello");
                                                                            return x*2;
                                                                        } );

                             // should NOT have been called.
                             REQUIRE( callCount==0 );

                             Thread::sleepSeconds(2);

                             // should STILL not have been called, since the wrapper was not executed yet
                             REQUIRE( callCount==0 );

                             StopWatch threadWatch;

                             std::future<int> result = wrapped(42);

                             // should NOT have been called immediately, since we are in a different thread.
                             // Instead the call should have been deferred to the main thread.
                             REQUIRE( callCount==0 );

                             // should not have waited
                             REQUIRE( threadWatch.getMillis()<500 );

                             REQUIRE( result.wait_for( std::chrono::milliseconds(5000) ) == std::future_status::ready );

                             // the inner function sleeps for 1 second.
                             REQUIRE( threadWatch.getMillis()>=1000-10 );
                             REQUIRE( threadWatch.getMillis()<2500 );

                             REQUIRE( callCount==1 );

                             REQUIRE( threadId==Thread::getMainId() );
                             REQUIRE( threadId!=Thread::getCurrentId() );

                             threadWatch.start();

                             if(throwException)
                                 REQUIRE_THROWS_AS(result.get(), InvalidArgumentError);
                             else
                                 REQUIRE( result.get()==84 );

                             // should not have waited
                             REQUIRE( threadWatch.getMillis()<=500 );

                             END_ASYNC_TEST();
                         } );


            // time to start thread should have been less than 1000ms
            REQUIRE( watch.getMillis()<1000 );

            MAKE_ASYNC_TEST(10);
        }

        SECTION("notStoringFuture")
        {
            MAKE_ASYNC_TEST(10);

            Thread::exec(
                         [throwException]()
                         {
                             struct Data : public Base
                             {
                                 volatile int callCount = 0;
                             };

                             P<Data> pData = newObj<Data>();

                             StopWatch threadWatch;

                             {
                                 auto wrapped = wrapCallFromMainThread<int>([pData, throwException](int x)
                                                                        {
                                                                            Thread::sleepMillis(2000);
                                                                            pData->callCount++;
                                                                            if(throwException)
                                                                                throw InvalidArgumentError("hello");
                                                                            return x*2;
                                                                        } );


                                 // should NOT have been called yet.
                                 REQUIRE( pData->callCount==0 );

                                 // should not have waited
                                 REQUIRE( threadWatch.getMillis()<500 );

                                 Thread::sleepSeconds(2);

                                 // should STILL not have been called, since the wrapper was not executed yet
                                 REQUIRE( pData->callCount==0 );

                                 threadWatch.start();

                                 wrapped(42);

                                 // should NOT have been called immediately, since we are in a different thread.
                                 // Instead the call should have been deferred to the main thread.
                                 REQUIRE( pData->callCount==0 );

                                 // should not have waited
                                 REQUIRE( threadWatch.getMillis()<500 );


                                 // wait a little
                                 Thread::sleepMillis(3000);

                                 // NOW the function should have been called
                                 REQUIRE( pData->callCount==1 );

                             }

                             // the other thread's pData reference should have been released
                             REQUIRE( pData->getRefCount()==1 );

                             END_ASYNC_TEST();
                         } );

        }
    }

#endif

}

TEST_CASE("wrapCallFromMainThread")
{
    SECTION("noException")
        testWrapCallFromMainThread(false);

    SECTION("exception")
        testWrapCallFromMainThread(true);
}







void testWrapAsyncCallFromMainThread(bool throwException)
{

    SECTION("mainThread")
    {
        struct Data : public Base
        {
            Thread::Id  threadId;
            int         callCount = 0;
        };
        P<Data> pData = newObj<Data>();

        StopWatch watch;

        auto wrapped = wrapAsyncCallFromMainThread<int>( [pData, throwException](int val)
                                                        {
                                                            pData->callCount++;
                                                            pData->threadId = Thread::getCurrentId();
                                                            
#if ! BDN_HAVE_THREADS
                                                            asyncCallFromMainThread([pData]()
                                                                                    {
                                                                                        // now the call should have happened.
                                                                                        REQUIRE( pData->callCount==1 );
                                                                                        REQUIRE( pData->threadId==Thread::getMainId() );
                                                                                        END_ASYNC_TEST();
                                                                                    } );
#endif
                                                            



                                                            if(throwException)
                                                                throw InvalidArgumentError("hello");

                                                            return val*2;
                                                        } );

        // should not have been called
        REQUIRE( pData->callCount==0 );

        wrapped(42);

        // should still not have been called (even though we are on the main thread).
        REQUIRE( pData->callCount==0 );

        // shoudl not have waited.
        REQUIRE( watch.getMillis()<500 );

        MAKE_ASYNC_TEST(10);
        
#if BDN_HAVE_THREADS

        Thread::exec( [pData]()
                      {
                          Thread::sleepMillis(2000);

                          // now the call should have happened.
                          REQUIRE( pData->callCount==1 );

                          REQUIRE( pData->threadId==Thread::getMainId() );

                          END_ASYNC_TEST();
                      } );
        
#endif
    }
    
#if BDN_HAVE_THREADS

    SECTION("otherThread")
    {
        MAKE_ASYNC_TEST(10);

        Thread::exec(
                     [throwException]()
                     {
                         volatile int   callCount = 0;
                         Thread::Id     threadId;

                         auto wrapped = wrapAsyncCallFromMainThread<int>([&callCount, throwException, &threadId](int x)
                                                                    {
                                                                        // sleep a little to ensure that we have time to check callCount
                                                                        Thread::sleepSeconds(1);
                                                                        threadId = Thread::getCurrentId();
                                                                        callCount++;
                                                                        if(throwException)
                                                                            throw InvalidArgumentError("hello");
                                                                        return x*2;
                                                                    } );

                         // should NOT have been called.
                         REQUIRE( callCount==0 );

                         Thread::sleepSeconds(2);

                         // should STILL not have been called, since the wrapper was not executed yet
                         REQUIRE( callCount==0 );

                         StopWatch threadWatch;

                         wrapped(42);

                         // should NOT have been called immediately, since we are in a different thread.
                         // Instead the call should have been deferred to the main thread.
                         REQUIRE( callCount==0 );

                         // should not have waited
                         REQUIRE( threadWatch.getMillis()<500 );

                         // sleep a while
                         Thread::sleepSeconds(3);

                         // now the call should have happened.
                         REQUIRE( callCount==1 );

                         REQUIRE( threadId==Thread::getMainId() );
                         REQUIRE( threadId!=Thread::getCurrentId() );


                         END_ASYNC_TEST();
                     } );
    }
    
#endif

}

TEST_CASE("wrapAsyncCallFromMainThread")
{
    SECTION("noException")
        testWrapAsyncCallFromMainThread(false);

    SECTION("exception")
        testWrapAsyncCallFromMainThread(true);
}


