//
// Created by matt on 27/3/20.
//

#ifndef NORMAL_NORMAL_SQL_SRC_S3SELECTCONNECTOR_H
#define NORMAL_NORMAL_SQL_SRC_S3SELECTCONNECTOR_H

#include <normal/sql/connector/Connector.h>

#include <normal/pushdown/AWSClient.h>
#include <normal/sql/connector/Connector.h>

namespace normal::sql::connector::s3 {

class S3SelectConnector : public normal::sql::connector::Connector {

private:
  std::shared_ptr<pushdown::AWSClient> awsClient_;

public:
  explicit S3SelectConnector(const std::string &name);
  ~S3SelectConnector() override = default;

  [[nodiscard]] const std::shared_ptr<pushdown::AWSClient> &getAwsClient() const;

};

}

#endif //NORMAL_NORMAL_SQL_SRC_S3SELECTCONNECTOR_H