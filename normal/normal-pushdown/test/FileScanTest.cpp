//
// Created by matt on 5/3/20.
//

#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <normal/pushdown/Collate.h>
#include <normal/core/OperatorManager.h>
#include <normal/pushdown/Aggregate.h>
#include <normal/pushdown/file/FileScan.h>
#include <normal/pushdown/aggregate/Sum.h>
#include <normal/pushdown/Project.h>
#include <normal/test/Globals.h>
#include <normal/expression/gandiva/Cast.h>
#include <normal/expression/gandiva/Column.h>
#include <normal/core/type/Float64Type.h>
#include <normal/test/TestUtil.h>

using namespace normal::core::type;
using namespace normal::expression;
using namespace normal::expression::gandiva;
using namespace normal::pushdown::aggregate;

#define SKIP_SUITE true

TEST_SUITE ("filescan" * doctest::skip(SKIP_SUITE)) {

TEST_CASE ("filescan-sum-collate" * doctest::skip(true || SKIP_SUITE)) {

  auto mgr = std::make_shared<normal::core::OperatorManager>();

  auto fileScan = std::make_shared<normal::pushdown::FileScan>("fileScan", "data/filescan/single-partition/test.csv");

  auto aggregateFunctions = std::make_shared<std::vector<std::shared_ptr<AggregationFunction>>>();

  // FIXME: Why does col need to be fully classified?

  aggregateFunctions->emplace_back(std::make_shared<Sum>("Sum",
														 cast(normal::expression::gandiva::col("A"), float64Type())));

  auto aggregate = std::make_shared<normal::pushdown::Aggregate>("aggregate", aggregateFunctions);
  auto collate = std::make_shared<normal::pushdown::Collate>("collate");

  fileScan->produce(aggregate);
  aggregate->consume(fileScan);

  aggregate->produce(collate);
  collate->consume(aggregate);

  mgr->put(fileScan);
  mgr->put(aggregate);
  mgr->put(collate);

  normal::test::TestUtil::writeExecutionPlan(*mgr);

  mgr->boot();

  mgr->start();
  mgr->join();

  auto tuples = collate->tuples();

  auto val = tuples->value<arrow::DoubleType>("Sum", 0);

	  CHECK(tuples->numRows() == 1);
	  CHECK(tuples->numColumns() == 1);
	  CHECK(val == 12.0);

  mgr->stop();
}

TEST_CASE ("filescan-project-collate" * doctest::skip(false || SKIP_SUITE)) {

  auto mgr = std::make_shared<normal::core::OperatorManager>();

  std::vector<std::string> columnNames = {"a","b","c"};
  auto fileScan = std::make_shared<normal::pushdown::FileScan>("fileScan", "data/filescan/single-partition/test.csv", columnNames , 0, 18);
  auto expressions = {
	  cast(normal::expression::gandiva::col("A"), float64Type()),
	  normal::expression::gandiva::col("B")
  };
  auto project = std::make_shared<normal::pushdown::Project>("project", expressions);
  auto collate = std::make_shared<normal::pushdown::Collate>("collate");

  fileScan->produce(project);
  project->consume(fileScan);

  project->produce(collate);
  collate->consume(project);

  mgr->put(fileScan);
  mgr->put(project);
  mgr->put(collate);

  normal::test::TestUtil::writeExecutionPlan(*mgr);

  mgr->boot();

  mgr->start();
  mgr->join();

  auto tuples = collate->tuples();

  SPDLOG_DEBUG("Output:\n{}", tuples->toString());

	  CHECK(tuples->numRows() == 3);
	  CHECK(tuples->numColumns() == 2);

  auto val_a_0 = tuples->value<arrow::DoubleType>("A", 0).value();
	  CHECK(val_a_0 == 1);
  auto val_a_1 = tuples->value<arrow::DoubleType>("A", 1).value();
	  CHECK(val_a_1 == 4);
  auto val_a_2 = tuples->value<arrow::DoubleType>("A", 2).value();
	  CHECK(val_a_2 == 7);

  auto val_b_0 = tuples->getString("B", 0).value();
	  CHECK(val_b_0 == "2");
  auto val_b_1 = tuples->getString("B", 1).value();
	  CHECK(val_b_1 == "5");
  auto val_b_2 = tuples->getString("B", 2).value();
	  CHECK(val_b_2 == "8");

  mgr->stop();
}

TEST_CASE ("filescan-sum-collate-parallel" * doctest::skip(true || SKIP_SUITE)) {

  auto mgr = std::make_shared<normal::core::OperatorManager>();

  auto fileScan01 = std::make_shared<normal::pushdown::FileScan>("fileScan01", "data/filescan/multi-partition/test01.csv");
  auto fileScan02 = std::make_shared<normal::pushdown::FileScan>("fileScan02", "data/filescan/multi-partition/test02.csv");
  auto fileScan03 = std::make_shared<normal::pushdown::FileScan>("fileScan03", "data/filescan/multi-partition/test03.csv");

  auto expressions01 = std::make_shared<std::vector<std::shared_ptr<AggregationFunction>>>();
  expressions01->emplace_back(std::make_shared<Sum>("sum(A)",
													cast(col("A"), float64Type())));
  auto aggregate01 = std::make_shared<normal::pushdown::Aggregate>("aggregate01", expressions01);

  auto expressions02 = std::make_shared<std::vector<std::shared_ptr<AggregationFunction>>>();
  expressions02->emplace_back(std::make_shared<Sum>("sum(A)",
													cast(col("A"), float64Type())));
  auto aggregate02 = std::make_shared<normal::pushdown::Aggregate>("aggregate02", expressions02);

  auto expressions03 = std::make_shared<std::vector<std::shared_ptr<AggregationFunction>>>();
  expressions03->emplace_back(std::make_shared<Sum>("sum(A)",
													cast(col("A"), float64Type())));
  auto aggregate03 = std::make_shared<normal::pushdown::Aggregate>("aggregate03", expressions03);

  auto reduceAggregateExpressions = std::make_shared<std::vector<std::shared_ptr<AggregationFunction>>>();
  reduceAggregateExpressions->emplace_back(std::make_shared<Sum>("sum(A)", col("sum(A)")));
  auto reduceAggregate = std::make_shared<normal::pushdown::Aggregate>("reduceAggregate", reduceAggregateExpressions);

  auto collate = std::make_shared<normal::pushdown::Collate>("collate");

  fileScan01->produce(aggregate01);
  aggregate01->consume(fileScan01);

  fileScan02->produce(aggregate02);
  aggregate02->consume(fileScan02);

  fileScan03->produce(aggregate03);
  aggregate03->consume(fileScan03);

  aggregate01->produce(reduceAggregate);
  reduceAggregate->consume(aggregate01);

  aggregate02->produce(reduceAggregate);
  reduceAggregate->consume(aggregate02);

  aggregate03->produce(reduceAggregate);
  reduceAggregate->consume(aggregate03);

  reduceAggregate->produce(collate);
  collate->consume(reduceAggregate);

  mgr->put(fileScan01);
  mgr->put(fileScan02);
  mgr->put(fileScan03);
  mgr->put(aggregate01);
  mgr->put(aggregate02);
  mgr->put(aggregate03);
  mgr->put(reduceAggregate);
  mgr->put(collate);

  normal::test::TestUtil::writeExecutionPlan(*mgr);

  mgr->boot();

  mgr->start();
  mgr->join();

  auto tuples = collate->tuples();

  auto val = tuples->value<arrow::DoubleType>("sum(A)", 0);

	  CHECK(tuples->numRows() == 1);
	  CHECK(tuples->numColumns() == 1);
	  CHECK(val == 36.0);

  mgr->stop();

}

}
