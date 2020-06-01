//
// Created by matt on 5/12/19.
//

#ifndef NORMAL_NORMAL_CORE_SRC_S3SELECTSCAN_H
#define NORMAL_NORMAL_CORE_SRC_S3SELECTSCAN_H

#include <memory>
#include <string>

#include <aws/s3/S3Client.h>
#include <normal/core/Cache.h>
#include <normal/pushdown/TupleMessage.h>
#include <normal/core/message/CompleteMessage.h>
#include <normal/tuple/TupleSet2.h>
#include <normal/core/cache/LoadResponseMessage.h>

#include "normal/core/Operator.h"
#include "normal/tuple/TupleSet.h"

using namespace normal::core;
using namespace normal::core::message;
using namespace normal::core::cache;

namespace normal::pushdown {

class S3SelectScan : public normal::core::Operator {

  typedef std::function<void(const std::shared_ptr<TupleSet2> &)> TupleSetEventCallback;

public:
  S3SelectScan(std::string name,
			   std::string s3Bucket,
			   std::string s3Object,
			   std::string sql,
			   std::vector<std::string> columnNames,
			   int64_t startOffset,
			   int64_t finishOffset,
			   std::shared_ptr<Aws::S3::S3Client> s3Client);

  std::shared_ptr<S3SelectScan> make(std::string name,
									 std::string s3Bucket,
									 std::string s3Object,
									 std::string sql,
									 std::vector<std::string> columnNames,
									 int64_t startOffset,
									 int64_t finishOffset,
									 std::shared_ptr<Aws::S3::S3Client> s3Client);

  void onReceive(const Envelope &message) override;
  void onTuple(const TupleMessage &message);
  void onComplete(const CompleteMessage &message);
  void onCacheLoadResponse(const LoadResponseMessage &Message);

private:
  std::string s3Bucket_;
  std::string s3Object_;
  std::string sql_;
  std::vector<std::string> columnNames_;
  int64_t startOffset_;
  int64_t finishOffset_;
  std::shared_ptr<Aws::S3::S3Client> s3Client_;
  std::vector<std::shared_ptr<Column>> columns_;

  void onStart();

  void requestSegmentsFromCache();

  tl::expected<void, std::string> s3Select(const TupleSetEventCallback &tupleSetEventCallback);

};

}

#endif //NORMAL_NORMAL_CORE_SRC_S3SELECTSCAN_H