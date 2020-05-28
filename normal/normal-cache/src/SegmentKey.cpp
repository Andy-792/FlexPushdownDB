//
// Created by matt on 19/5/20.
//

#include "normal/cache/SegmentKey.h"

using namespace normal::cache;

SegmentKey::SegmentKey(std::shared_ptr<Partition> Partition, std::string columnName, SegmentRange Range)
	: partition_(std::move(Partition)), columnName_(std::move(columnName)), range_(Range) {}

std::shared_ptr<SegmentKey> SegmentKey::make(const std::shared_ptr<Partition> &Partition,
											 std::string columnName,
											 SegmentRange Range) {
  return std::make_shared<SegmentKey>(std::move(Partition), std::move(columnName), Range);
}

const std::shared_ptr<Partition> &SegmentKey::getPartition() const {
  return partition_;
}

[[maybe_unused]] const SegmentRange &SegmentKey::getRange() const {
  return range_;
}

std::string SegmentKey::toString() {
  return fmt::format("({},{})", partition_->toString(), range_.toString());
}

bool SegmentKey::operator==(const SegmentKey &other) const {
  return this->partition_->equalTo(other.partition_) &&
	  this->columnName_ == other.columnName_ &&
	  this->range_ == other.range_;
}

bool SegmentKey::operator!=(const SegmentKey &other) const {
  return !(*this == other);
}

size_t SegmentKey::hash() {
  return partition_->hash() + range_.hash();
}

const std::string &SegmentKey::getColumnName() const {
  return columnName_;
}