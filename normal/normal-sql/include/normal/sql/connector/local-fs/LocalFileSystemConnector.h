//
// Created by matt on 27/3/20.
//

#ifndef NORMAL_NORMAL_SQL_INCLUDE_NORMAL_SQL_CONNECTOR_LOCAL_FS_LOCALFILESYSTEMCONNECTOR_H
#define NORMAL_NORMAL_SQL_INCLUDE_NORMAL_SQL_CONNECTOR_LOCAL_FS_LOCALFILESYSTEMCONNECTOR_H

#include <normal/sql/connector/Connector.h>

namespace normal::sql::connector::local_fs {

class LocalFileSystemConnector : public normal::sql::connector::Connector {

private:

public:
  explicit LocalFileSystemConnector(const std::string &Name);
  ~LocalFileSystemConnector() override = default;
};

}

#endif //NORMAL_NORMAL_SQL_INCLUDE_NORMAL_SQL_CONNECTOR_LOCAL_FS_LOCALFILESYSTEMCONNECTOR_H