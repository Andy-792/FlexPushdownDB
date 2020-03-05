//
// Created by matt on 5/3/20.
//

#include <string>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <arrow/array/builder_binary.h>           // for StringBuilder
#include <arrow/table.h>                          // for Table
#include <arrow/type.h>                           // for field, schema, Schema
#include <arrow/type_fwd.h>                       // for default_memory_pool
#include <memory.h>                      // for shared_ptr, make_sh...
#include <cstdio>                                // for FILENAME_MAX
#include <unistd.h>                               // for getcwd
#include <iostream>                               // for cout

#include "normal/pushdown/S3SelectScan.h"
#include "normal/pushdown/Collate.h"
#include "normal/core/OperatorContext.h"
#include <normal/core/OperatorManager.h>
#include <normal/pushdown/Aggregate.h>
#include <normal/pushdown/FileScan.h>
#include <normal/core/Normal.h>
#include <aws/core/Aws.h>
#include <aws/core/utils/ratelimiter/DefaultRateLimiter.h>
#include <aws/core/utils/threading/Executor.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <normal/pushdown/AWSClient.h>
#include "normal/core/TupleSet.h"                 // for TupleSet
#include "normal/pushdown/AggregateExpression.h"  // for AggregateExpression

#include "Globals.h"

namespace arrow { class Array; }
namespace arrow { class MemoryPool; }
namespace arrow { class StringArray; }

auto fn = [](std::shared_ptr<normal::core::TupleSet> dataTupleSet,
             std::shared_ptr<normal::core::TupleSet> aggregateTupleSet) -> std::shared_ptr<normal::core::TupleSet> {

  SPDLOG_DEBUG("Data:\n{}", dataTupleSet->toString());

  std::string sum = dataTupleSet->visit([](std::string accum, arrow::RecordBatch &batch) -> std::string {
    auto fieldIndex = batch.schema()->GetFieldIndex("f5");
    std::shared_ptr<arrow::Array> array = batch.column(fieldIndex);

    double sum = 0;
    if (accum.empty()) {
      sum = 0;
    } else {
      sum = std::stod(accum);
    }

    std::shared_ptr<arrow::DataType> colType = array->type();
    if (colType->Equals(arrow::Int64Type())) {
      std::shared_ptr<arrow::Int64Array>
          typedArray = std::static_pointer_cast<arrow::Int64Array>(array);
      for (int i = 0; i < batch.num_rows(); ++i) {
        long val = typedArray->Value(i);
        sum += val;
      }
    } else if (colType->Equals(arrow::StringType())) {
      std::shared_ptr<arrow::StringArray>
          typedArray = std::static_pointer_cast<arrow::StringArray>(array);
      for (int i = 0; i < batch.num_rows(); ++i) {
        std::string val = typedArray->GetString(i);
        sum += std::stod(val);
      }
    } else if (colType->Equals(arrow::DoubleType())) {
      std::shared_ptr<arrow::DoubleArray>
          typedArray = std::static_pointer_cast<arrow::DoubleArray>(array);
      for (int i = 0; i < batch.num_rows(); ++i) {
        double val = typedArray->Value(i);
        sum += val;
      }
    } else {
      abort();
    }

    std::stringstream ss;
    ss << sum;
    return std::string(ss.str());
  });

  // Create new aggregate tuple set
  std::vector<std::shared_ptr<std::string>> data;
  data.push_back(std::make_shared<std::string>(sum));

  std::shared_ptr<arrow::Schema> schema;

  std::shared_ptr<arrow::Field> field;
  field = arrow::field("sum(f5)", arrow::utf8());

  schema = arrow::schema({field});

  SPDLOG_DEBUG("\n" + schema->ToString());

  arrow::MemoryPool *pool = arrow::default_memory_pool();
  arrow::StringBuilder colBuilder(pool);

  colBuilder.Append(sum);

  std::shared_ptr<arrow::StringArray> col;
  colBuilder.Finish(&col);

  auto columns = std::vector<std::shared_ptr<arrow::Array>>{col};

  std::shared_ptr<arrow::Table> table;
  table = arrow::Table::Make(schema, columns);

  aggregateTupleSet = normal::core::TupleSet::make(table);

  return aggregateTupleSet;
};

TEST_CASE ("S3SelectScan -> Sum -> Collate") {

  normal::pushdown::AWSClient client;
  client.init();

  char buff[FILENAME_MAX];
  getcwd(buff, FILENAME_MAX);
  std::string current_working_dir(buff);

  SPDLOG_DEBUG("Current working dir: {}", current_working_dir);

  auto aggregateExpression = std::make_unique<normal::pushdown::AggregateExpression>(fn);
  auto aggregateExpressions = std::vector<std::unique_ptr<normal::pushdown::AggregateExpression>>();
  aggregateExpressions.push_back(std::move(aggregateExpression));

  auto s3selectScan = std::make_shared<normal::pushdown::S3SelectScan>("s3SelectScan",
                                                                       "s3filter",
                                                                       "tpch-sf1/customer.csv",
                                                                       "select * from S3Object limit 1000",
                                                                       client.defaultS3Client());
  auto aggregate = std::make_shared<normal::pushdown::Aggregate>("aggregate", std::move(aggregateExpressions));
  auto collate = std::make_shared<normal::pushdown::Collate>("collate");

  s3selectScan->produce(aggregate);
  aggregate->consume(s3selectScan);

  aggregate->produce(collate);
  collate->consume(aggregate);

  auto mgr = std::make_shared<OperatorManager>();

  mgr->put(s3selectScan);
  mgr->put(aggregate);
  mgr->put(collate);

  mgr->start();
  mgr->join();

  auto tuples = collate->tuples();

  auto val = tuples->getValue("sum(f5)", 0);

      CHECK(tuples->numRows() == 1);
      CHECK(tuples->numColumns() == 1);
      CHECK(val == "4.40023e+06");

  mgr->stop();

  client.shutdown();
}