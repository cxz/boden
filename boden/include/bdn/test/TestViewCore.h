#ifndef BDN_TEST_TestViewCore_H_
#define BDN_TEST_TestViewCore_H_

#include <bdn/View.h>
#include <bdn/Window.h>
#include <bdn/test.h>
#include <bdn/IUiProvider.h>
#include <bdn/RequireNewAlloc.h>
#include <bdn/Button.h>

namespace bdn
{
namespace test
{


/** Helper for tests that verify IViewCore implementations.*/
class TestViewCore : public RequireNewAlloc<Base, TestViewCore>
{
public:

    /** Performs the tests.*/
    virtual void runTests()
    {
        _pWindow = newObj<Window>( &getUiProvider() );

        _pWindow->visible() = true;

        setView( createView() );

        // sanity check: the view should not have a parent yet
        REQUIRE( _pView->getParentView()==nullptr );

        SECTION("init")
        {      
            if( _pView == cast<View>(_pWindow) )
            {
                // the view is a window. These always have a core from
                // the start, so we cannot do any init tests with them.

                // only check that the view core is indeed already there.
                REQUIRE(_pView->getViewCore()!=nullptr );
            }
            else
            {
                // non-windows should not have a view core in the beginning
                // (before they are added to the window).

                REQUIRE(_pView->getViewCore()==nullptr );

                // run the init tests for them
                runInitTests();
            }
        }

        SECTION("postInit")
        {
            initCore();

            // view should always be visible for these tests
            _pView->visible() = true;

            // ensure that all pending initializations have finished.
            P<TestViewCore> pThis = this;
            CONTINUE_SECTION_AFTER_PENDING_EVENTS(pThis)
            {
                pThis->runPostInitTests();
            };
        }
    }    

protected:

    /** Returns true if the view position can be manually changed.
        Returns false if this is a UI element whose position is controlled
        by an external entity.
        
        The default implementation returns true
        */
    virtual bool canManuallyChangePosition() const
    {
        return true;
    }


    /** Returns true if the view sizecan be manually changed.
        Returns false if this is a UI element whose size is controlled
        by an external entity.
        
        The default implementation returns true
        */
    virtual bool canManuallyChangeSize() const
    {
        return true;
    }

    /** Runs the tests that verify that the core initializes itself with the current
        property values of the outer view.

        The core is not yet initialized when this function is called
        
        The tests each modify an outer view property, then cause the core to be created
        (by calling initCore()) and then verify that the core has initialized itself correctly.
        */
    virtual void runInitTests()
    {
        SECTION("visible")
        {
            _pView->visible() = true;

            initCore();
            verifyCoreVisibility();
        }
    
        SECTION("invisible")
        {
            _pView->visible() = false;

            initCore();
            verifyCoreVisibility();
        }

        SECTION("padding")
        {
            SECTION("default")
            {
                // the default padding of the outer view should be null (i.e. "use default").
                REQUIRE( _pView->padding().get().isNull() );

                initCore();
                verifyCorePadding();
            }

            SECTION("explicit")
            {
                _pView->padding() = UiMargin( UiLength::sem, 11, 22, 33, 44);

                initCore();
                verifyCorePadding();
            }
        }

        SECTION("position")
        {
            _pView->position() = Point(110, 220);

            initCore();
            verifyInitialDummyCorePosition();
        }

        SECTION("size")
        {
            _pView->size() = Size(880, 990);

            initCore();
            verifyInitialDummyCoreSize();
        }
    }

    /** Runs the tests that verify the core behaviour for operations that happen
        after the core is initialized.
        
        The core is already created/initialized when this is function is called.
        */
    virtual void runPostInitTests()
    {        
        SECTION("uiLengthToDips")
        {
            REQUIRE( _pCore->uiLengthToDips( UiLength(UiLength::dip, 0) ) == 0 );
            REQUIRE( _pCore->uiLengthToDips( UiLength(UiLength::sem, 0) ) == 0 );

            REQUIRE( _pCore->uiLengthToDips( UiLength(UiLength::dip, 17.34) ) == 17.34 );
            
            double semSize = _pCore->uiLengthToDips( UiLength(UiLength::sem, 1) );
            REQUIRE( semSize>0 );
            REQUIRE_ALMOST_EQUAL( _pCore->uiLengthToDips( UiLength(UiLength::sem, 3) ), semSize*3, 3);
        }

        SECTION("uiMarginToDipMargin")
        {
            SECTION("dip")
            {
                REQUIRE( _pCore->uiMarginToDipMargin( UiMargin(UiLength::dip, 10, 20, 30, 40) ) == Margin(10, 20, 30, 40) );
            }

            SECTION("sem")
            {
                double semDips = _pCore->uiLengthToDips( UiLength(UiLength::sem, 1) );

                Margin m = _pCore->uiMarginToDipMargin( UiMargin(UiLength::sem, 10, 20, 30, 40) );
                REQUIRE_ALMOST_EQUAL( m.top, 10*semDips, 10);
                REQUIRE_ALMOST_EQUAL( m.right, 20*semDips, 20);
                REQUIRE_ALMOST_EQUAL( m.bottom, 30*semDips, 30);
                REQUIRE_ALMOST_EQUAL( m.left, 40*semDips, 40);
            }
        }

        
        if(coreCanCalculatePreferredSize())
        {	
            SECTION("preferredSize")
            {
	            SECTION("calcPreferredSize plausible")	
                {
                    // we check elsewhere that padding is properly included in the preferred size
                    // So here we only check that the preferred size is "plausible"

                    Size prefSize = _pCore->calcPreferredSize();

                    REQUIRE( prefSize.width>=0 );
                    REQUIRE( prefSize.height>=0 );
                }
                
                SECTION("availableSize same as preferredSize")	
                {
                    SECTION("no padding")
                    {
                        // do nothing
                    }
                    
                    SECTION("with padding")
                    {
                        _pView->padding() = UiMargin( UiLength::Unit::dip, 10, 20, 30, 40);
                    }
                    
                    Size prefSize = _pCore->calcPreferredSize();
                    
                    Size prefSizeRestricted = _pCore->calcPreferredSize( prefSize.width, prefSize.height);
                    
                    REQUIRE( prefSize == prefSizeRestricted);
                }
                                
                SECTION("calcPreferredSize restrictedWidth plausible")	
                {
                    // this is difficult to test, since it depends heavily on what kind of view
                    // we actually work with. Also, it is perfectly normal for different core implementations
                    // to have different preferred size values for the same inputs.

                    // So we can only test rough plausibility here.
                    Size prefSize = _pCore->calcPreferredSize();

                    SECTION("unconditionalWidth")
                    {
                        // When we specify exactly the unconditional preferred width then we should get exactly the unconditional preferred height
                        REQUIRE( _pCore->calcPreferredSize(prefSize.width).height == prefSize.height );
                    }

                    SECTION("zero")
                    {
                        REQUIRE( _pCore->calcPreferredSize(0).height >= prefSize.height );
                    }
                }

                SECTION("calcPreferredSize restrictedHeight plausible")	
                {
                    Size prefSize = _pCore->calcPreferredSize();

                    SECTION("unconditionalHeight")
                        REQUIRE( _pCore->calcPreferredSize(-1, prefSize.height).width == prefSize.width );
        
                    SECTION("zero")
                        REQUIRE( _pCore->calcPreferredSize(-1, 0).width >= prefSize.width );
                }
            }
        }
    
        SECTION("visibility")   
        {
            SECTION("visible")
            {
                _pView->visible() = true;
                verifyCoreVisibility();
            }

            SECTION("invisible")
            {
                _pView->visible() = false;
                verifyCoreVisibility();
            }

            if(coreCanCalculatePreferredSize())                
            {
                SECTION("noEffectOnPreferredSize")
                {
                    // verify that visibility has no effect on the preferred size
                    Size prefSizeBefore = _pCore->calcPreferredSize();

                    _pView->visible() = true;
                    REQUIRE( _pCore->calcPreferredSize() == prefSizeBefore );

                    _pView->visible() = false;
                    REQUIRE( _pCore->calcPreferredSize() == prefSizeBefore );

                    _pView->visible() = true;
                    REQUIRE( _pCore->calcPreferredSize() == prefSizeBefore );
                }
            }
        }

        SECTION("padding")
        {
            SECTION("custom")
            {
                _pView->padding() = UiMargin( UiLength::dip, 11, 22, 33, 44);
                verifyCorePadding();
            }

            SECTION("default after custom")
            {
                // set a non-default padding, then go back to default padding.
                _pView->padding() = UiMargin( UiLength::dip, 11, 22, 33, 44);
                _pView->padding() = nullptr;

                verifyCorePadding();
            }

            if(coreCanCalculatePreferredSize())
            {
                SECTION("effectsOnPreferredSize")
                {
                    // For some UI elements on some platforms there may be a silent minimum
                    // padding. If we specify a smaller padding then the minimum will be used
                    // instead.
        
                    // So to verify the effects of padding we first set a big padding that
                    // we are fairly confident to be over any minimum.
        
                    UiMargin paddingBefore(UiLength::sem, 10, 10, 10, 10);

                    _pView->padding() = paddingBefore;
                    
                    // wait a little so that sizing info is updated.
                    // Note that on some platforms CONTINUE_SECTION_AFTER_PENDING_EVENTS is not good enough
                    // because the sizing updates happen with a low priority.
                    // So we explicitly wait a little bit.
                    P<TestViewCore> pThis = this;
                    CONTINUE_SECTION_AFTER_SECONDS(0.5, pThis, paddingBefore )
                    {        
                        Size prefSizeBefore = pThis->_pCore->calcPreferredSize();

                        UiMargin additionalPadding(UiLength::sem, 1, 2, 3, 4);
                        UiMargin increasedPadding = UiMargin(
                            UiLength::sem,
                            paddingBefore.top.value + additionalPadding.top.value,
                            paddingBefore.right.value + additionalPadding.right.value,
                            paddingBefore.bottom.value + additionalPadding.bottom.value,
                            paddingBefore.left.value + additionalPadding.left.value );

                        // setting padding should increase the preferred size
                        // of the core.
                        pThis->_pView->padding() = increasedPadding;


                        CONTINUE_SECTION_AFTER_PENDING_EVENTS( pThis, prefSizeBefore, additionalPadding )
                        {        
                            // the padding should increase the preferred size.
                            Size prefSize = pThis->_pCore->calcPreferredSize();

                            Margin  additionalPaddingPixels = pThis->_pView->uiMarginToDipMargin(additionalPadding);

                            REQUIRE_ALMOST_EQUAL( prefSize, prefSizeBefore+additionalPaddingPixels, Size(1,1) );
                        };
                    };
                }
            }
        }

        SECTION("position")
        {
            SECTION("manualChange")
            {
                if(canManuallyChangePosition())
                {
					_pView->position() = Point(110, 220);

                    // it may take a layout cycle until the bounds have updated
                    P<TestViewCore> pThis = this;
                    CONTINUE_SECTION_AFTER_PENDING_EVENTS(pThis)
                    {
                        pThis->verifyCorePosition();
                    };
                }
                else
                {
                    // when the control does not have control over its own position then there can be
                    // a delay in the processing.
                    // We must ensure that the control has finished its initial initialization before
                    // we continue. That might take some time in some ports - and a simple
                    // CONTINUE_SECTION_AFTER_PENDING_EVENTS is not enough on all platforms (e.g. winuwp).
                    // So we use CONTINUE_SECTION_AFTER_SECONDS instead
                    P<TestViewCore> pThis = this;

                    CONTINUE_SECTION_AFTER_SECONDS( 2, pThis )
                    {
                        // the control cannot manually change its position.
                        // In that case the core must reset the position property back
                        // to what it was originally. This reset may be done in a scheduled async call,
                        // so we must process pending events before we test for it.
                        Point origPosition = pThis->_pView->position();
                        
                        // sanity check: at this point the core bounds should always match
                        pThis->verifyCorePosition();

                        pThis->_pView->position() = Point(117, 227);

                        // again, we must wait until the changes have propagated
                        CONTINUE_SECTION_AFTER_SECONDS(2, pThis, origPosition )
                        {
                            REQUIRE( pThis->_pView->position().get() == origPosition );

                            pThis->verifyCorePosition();
                        };
                    };
                }
            }
        }


        SECTION("size")
        {
            SECTION("manualChange")
            {
                if(canManuallyChangeSize())
                {
					// note: don't get too big here. If we exceed the screen size then
					// the window size be clipped by the OS.
                    _pView->size() = Size(550, 330);

                    // it may take a layout cycle until the bounds have updated
                    P<TestViewCore> pThis = this;
                    CONTINUE_SECTION_AFTER_PENDING_EVENTS(pThis)
                    {
                        pThis->verifyCoreSize();
                    };
                }
                else
                {
                    // when the control does not have control over its own size then there can be
                    // a delay in the processing.
                    // We must ensure that the control has finished its initial initialization before
                    // we continue. That might take some time in some ports - and a simple
                    // CONTINUE_SECTION_AFTER_PENDING_EVENTS is not enough on all platforms (e.g. winuwp).
                    // So we use CONTINUE_SECTION_AFTER_SECONDS instead
                    P<TestViewCore> pThis = this;

                    CONTINUE_SECTION_AFTER_SECONDS( 2, pThis )
                    {
                        // the control cannot manually change its size.
                        // In that case the core must reset the size property back
                        // to what it was originally. This reset may be done in a scheduled async call,
                        // so we must process pending events before we test for it.
                        Size origSize = pThis->_pView->size();

                        // sanity check: at this point the core size should always match
                        pThis->verifyCoreSize();

                        pThis->_pView->size() = Size(887, 997);

                        // again, we must wait until the changes have propagated
                        CONTINUE_SECTION_AFTER_SECONDS(2, pThis, origSize )
                        {
                            REQUIRE( pThis->_pView->size().get() == origSize );

                            pThis->verifyCoreSize();
                        };
                    };
                }
            }

            if(coreCanCalculatePreferredSize())
            {
                SECTION("noEffectOnPreferredSize")
                {
                    Size prefSizeBefore = _pCore->calcPreferredSize();

                    _pView->size() = Size(300, 400);
                    
                    REQUIRE( _pCore->calcPreferredSize() == prefSizeBefore );

                    _pView->size() = Size(3000, 4000);
                    
                    REQUIRE( _pCore->calcPreferredSize() == prefSizeBefore );
                }
            }
        }
    }


    /** Causes the core object to be created. This is done by adding the view as
        a child to a visible view container or window.*/
    virtual void initCore()
    {
        if(_pView!=cast<View>(_pWindow))
            _pWindow->setContentView( _pView );

        _pCore = _pView->getViewCore();

        REQUIRE( _pCore!=nullptr );
    }

    /** Verifies that the core's visible property matches that of the outer view.*/
    virtual void verifyCoreVisibility()=0;

    /** Verifies that the core's padding property matches that of the outer view.*/
    virtual void verifyCorePadding()=0;


    /** Verifies that the core's position property has the initial dummy value used
        directly after initialization.*/
    virtual void verifyInitialDummyCorePosition()=0;


    /** Verifies that the core's size property has the initial dummy value used
        directly after initialization.*/
    virtual void verifyInitialDummyCoreSize()=0;


    /** Verifies that the core's position property matches that of the outer view.*/
    virtual void verifyCorePosition()=0;

    /** Verifies that the core's size property matches that of the outer view.*/
    virtual void verifyCoreSize()=0;


    /** Returns the UiProvider to use.*/
    virtual IUiProvider& getUiProvider()=0;


    /** Creates the view object to use for the tests.*/
    virtual P<View> createView()=0;

    /** Sets the view object to use for the tests.*/
    virtual void setView(View* pView)
    {
        _pView = pView;
    }


    /** Returns true if the view core can calculate its preferred size.
        Some core types depend on the outer view to calculate the preferred size
        instead.
        
        The default implementation returns true.
        */
    virtual bool coreCanCalculatePreferredSize()
    {
        return true;
    }

    
    P<Window> _pWindow;
    P<View>   _pView;

    P<IViewCore> _pCore;
};



}
}

#endif

