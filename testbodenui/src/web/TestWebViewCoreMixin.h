#ifndef BDN_TEST_TestWebViewCoreMixin_H_
#define BDN_TEST_TestWebViewCoreMixin_H_


#include <bdn/View.h>
#include <bdn/web/ViewCore.h>
#include <bdn/web/UiProvider.h>

namespace bdn
{
namespace test
{

    
/** A mixin class that adds implementations of web view specific functionality on top of
    the base class specified in the template parameter BaseClass.*/
template<class BaseClass>
class TestWebViewCoreMixin : public BaseClass
{
public:
    TestWebViewCoreMixin()
    : _domObject( emscripten::val::null() )
    {        
    }

protected:

    void initCore() override
    {
        BaseClass::initCore();

        _pWebCore = cast<bdn::web::ViewCore>( BaseClass::_pView->getViewCore() );
        REQUIRE( _pWebCore!=nullptr );

        _domObject = _pWebCore->getDomObject();
        REQUIRE( !_domObject.isNull() );
    }

    IUiProvider& getUiProvider() override
    {
        return bdn::web::UiProvider::get();
    }

    void verifyCoreVisibility() override
    {
        bool expectedVisible = BaseClass::_pView->visible();

        String vis = _domObject["style"]["visibility"].template as<std::string>();
        REQUIRE( vis == String(expectedVisible ? "visible" : "hidden") );
    }

    std::string getValueAsString(emscripten::val obj, const char* valueName)
    {
        emscripten::val valueObj = obj[valueName];
        if(valueObj.isUndefined() || valueObj.isNull())
            return "";

        return valueObj.as<std::string>();
    }

    int getValueAsPixelInt(emscripten::val obj, const char* valueName)
    {
        std::string val = getValueAsString(obj, valueName);

        // do not use as<int>. It can cause crashes when the value is incompatible
        if(val.empty())
            return 0;

        // if the string ends with "px" then cut it off
        if(val.length()>=2 && val.substr( val.length()-2, 2)=="px")
            val = val.substr( 0, val.length()-2 );

        for(auto c: val)
        {
            if(!isdigit(c))
                return 0;
        }

        return std::stoi(val);
    }

    virtual Rect getViewRect()
    {
        // bounds should translate to style setting

        emscripten::val style = _domObject["style"];

        if(style.isNull() || style.isUndefined())
            return Rect();
        else
        {
            int width = getValueAsPixelInt(style, "width");
            int height = getValueAsPixelInt(style, "height");
            int x = getValueAsPixelInt(style, "left");
            int y = getValueAsPixelInt(style, "top");

            return Rect(x, y, width, height);
        }
    }

    void verifyInitialDummyCoreBounds() override
    {        
        Rect rect = getViewRect();

        REQUIRE( rect == Rect() );
    }

    void verifyCoreBounds() override
    {        
        Rect rect = getViewRect();
        Rect expectedRect = BaseClass::_pView->bounds();

        REQUIRE( rect == expectedRect );
    }


    void verifyCorePadding() override
    {
        emscripten::val styleObj = _domObject["style"];

        REQUIRE( !styleObj.isNull() );
        REQUIRE( !styleObj.isUndefined() );
        
        emscripten::val pad = styleObj["padding"];
        
        Nullable<UiMargin> expectedPad = BaseClass::_pView->padding();
        if(expectedPad.isNull())
        {
            if(!pad.isUndefined())
                std::cout << "Padding: '"+pad.as<std::string>()<<"'" << std::endl;
            REQUIRE( pad.isUndefined() );
        }
        else
        {            
            REQUIRE( !pad.isNull() );
            REQUIRE( !pad.isUndefined() );

            std::string padString = pad.as<std::string>();

            Margin expectedPixelPadding = BaseClass::_pView->uiMarginToPixelMargin( expectedPad );

            String expectedPadString = std::to_string(expectedPixelPadding.top)+"px "
                                        + std::to_string(expectedPixelPadding.right)+"px "
                                        + std::to_string(expectedPixelPadding.bottom)+"px "
                                        + std::to_string(expectedPixelPadding.left)+"px";

            REQUIRE( padString == expectedPadString );
        }
    }


    P<bdn::web::ViewCore> _pWebCore;
    emscripten::val       _domObject;
};



}
}



#endif

