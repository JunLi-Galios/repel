#define BOOST_TEST_MODULE SpanInterval 
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include "../src/spaninterval.h"
#include "../src/interval.h"


BOOST_AUTO_TEST_CASE( constructors_test )
{
  SpanInterval sp(0,0,0,0);
  Interval start = sp.start();
  Interval end = sp.end();
  BOOST_CHECK_EQUAL(start.start(), 0);
  BOOST_CHECK_EQUAL(start.end(), 0);
  BOOST_CHECK_EQUAL(end.start(), 0);
  BOOST_CHECK_EQUAL(end.end(), 0);
}
