#include <bdn/init.h>

#include <bdn/UiLength.h>

#include <bdn/test.h>


using namespace bdn;

TEST_CASE("UiLength")
{
	SECTION("defaultConstruct")
	{
		UiLength a;

		REQUIRE( a.unit == UiLength::Unit::sem );
		REQUIRE( a.value == 0.0 );
	}

	SECTION("constructUnitValue")
	{
		UiLength a(UiLength::Unit::dip, 12.3456 );

		REQUIRE( a.unit == UiLength::Unit::dip );
		REQUIRE( a.value == 12.3456);
	}

	SECTION("equality")
	{
		UiLength a(UiLength::Unit::dip, 12.3456 );
		
		checkEquality( UiLength(), UiLength(), true );
		checkEquality( a, UiLength(), false );
		checkEquality( a, UiLength(UiLength::Unit::dip, 12.3456), true );
		checkEquality( a, UiLength(UiLength::Unit::realPixel, 12.3456), false );
		checkEquality( a, UiLength(UiLength::Unit::dip, 17), false );
	}

}


