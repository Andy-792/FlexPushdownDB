//
// Created by matt on 7/4/20.
//

#include <connector/s3/S3SelectCatalogueEntry.h>
#include <normal/pushdown/S3SelectScan.h>
#include <normal/pushdown/FileScan.h>
#include "S3SelectScanLogicalOperator.h"

S3SelectScanLogicalOperator::S3SelectScanLogicalOperator(const std::string &S3Bucket,
                                                         const std::string &S3Object,
                                                         const std::shared_ptr<normal::pushdown::AWSClient> &AwsClient)
    : s3Bucket_(S3Bucket), s3Object_(S3Object), awsClient_(AwsClient) {}

std::shared_ptr<normal::core::Operator> S3SelectScanLogicalOperator::toOperator() {
  return std::make_shared<normal::pushdown::S3SelectScan>("s3scan", this->s3Bucket_, this->s3Object_, "select * from S3Object", this->s3Object_, "A", this->awsClient_->defaultS3Client());
}

