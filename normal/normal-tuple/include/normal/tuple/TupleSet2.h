//
// Created by matt on 1/5/20.
//

#ifndef NORMAL_NORMAL_TUPLE_INCLUDE_NORMAL_TUPLE_TUPLESET2_H
#define NORMAL_NORMAL_TUPLE_INCLUDE_NORMAL_TUPLE_TUPLESET2_H

#include <vector>

#include <normal/core/TupleSet.h>
#include "Column.h"
#include "Schema.h"

namespace normal::tuple {

class TupleSet2 : public std::enable_shared_from_this<TupleSet2> {

public:

  /**
   * Creates an empty tuple set
   */
  explicit TupleSet2();

  /**
   * Creates a tuple set from an arrow table
   *
   * @param arrowTable
   */
  explicit TupleSet2(std::shared_ptr<::arrow::Table> arrowTable);

  /**
   * Creates a tuple set from a v1 tuple set
   *
   * @param tuples
   * @return
   */
  static std::shared_ptr<TupleSet2> create(const std::shared_ptr<normal::core::TupleSet>& tuples);

  /**
   * Gets the tuple set as a v1 tuple set
   * @return
   */
  std::shared_ptr<normal::core::TupleSet> toTupleSetV1();

  /**
   * Returns number of rows in the tuple set
   * @return
   */
  long numRows();

  /**
   * Clears the schema and all data
   */
  void clear();

  /**
   * Concatenates a vector of tuple sets into a new single tupleset
   *
   * @param tuples
   * @return
   */
  static tl::expected<std::shared_ptr<TupleSet2>, std::string> concatenate(const std::vector<std::shared_ptr<TupleSet2>>& tupleSets);

  /**
   * Appends a vector of tuple sets to this tuple set and returns the new tuple set
   *
   * @param tuples
   * @return
   */
  tl::expected<void, std::string> append(const std::vector<std::shared_ptr<TupleSet2>>& tupleSet);

  /**
   * Appends a tuple set to this tuple set and returns the new tuple set
   *
   * @param tuples
   * @return
   */
  tl::expected<void, std::string> append(const std::shared_ptr<TupleSet2>& tupleSet);

  /**
   * Returns a single column by name
   *
   * @return
   */
  tl::expected<std::shared_ptr<Column>, std::string> getColumnByName(const std::string &columnName);

  /**
   * Returns the tuple set pretty printed as a string
   *
   * @return
   */
  std::string showString();

  /**
   * The tuple set schema
   *
   * @return
   */
  std::shared_ptr<Schema> schema() {
    return std::make_shared<Schema>(table_.value()->schema());
  }

private:

  /**
   * The underlying arrow table, which may not be set to support an "empty" tuple set
   */
  std::optional<std::shared_ptr<::arrow::Table>> table_;

  static std::vector<std::shared_ptr<arrow::Table>> tupleSetVectorToArrowTableVector(const std::vector<std::shared_ptr<TupleSet2>> &tupleSets);
};

}

#endif //NORMAL_NORMAL_TUPLE_INCLUDE_NORMAL_TUPLE_TUPLESET2_H