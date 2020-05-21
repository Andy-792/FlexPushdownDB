//
// Created by matt on 8/5/20.
//

#ifndef NORMAL_NORMAL_EXPRESSION_GANDIVA_TEST_TESTUTIL_H
#define NORMAL_NORMAL_EXPRESSION_GANDIVA_TEST_TESTUTIL_H

#include <memory>

#include <normal/tuple/TupleSet.h>

namespace normal::expression::gandiva::test {

class TestUtil {

public:

/**
 * Create a simple 3 col, 3 row tupleset for testing expressions against
 *
 * @return
 */
  static std::shared_ptr<normal::core::TupleSet> prepareTupleSet() {

	auto column1 = std::vector{"1", "2", "3"};
	auto column2 = std::vector{"4", "5", "6"};
	auto column3 = std::vector{"7", "8", "9"};

	auto stringType = arrow::TypeTraits<arrow::StringType>::type_singleton();

	auto fieldA = field("a", stringType);
	auto fieldB = field("b", stringType);
	auto fieldC = field("c", stringType);
	auto schema = arrow::schema({fieldA, fieldB, fieldC});

	auto arrowColumn1 = Arrays::make<arrow::StringType>(column1).value();
	auto arrowColumn2 = Arrays::make<arrow::StringType>(column2).value();
	auto arrowColumn3 = Arrays::make<arrow::StringType>(column3).value();

	auto tuples = normal::core::TupleSet::make(schema, {arrowColumn1, arrowColumn2, arrowColumn3});

	return tuples;
  }
};

}

#endif //NORMAL_NORMAL_EXPRESSION_GANDIVA_TEST_TESTUTIL_H
