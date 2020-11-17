//
// Created by matt on 26/3/20.
//

#include <normal/sql/Interpreter.h>

#include <normal/sql/NormalSQLLexer.h>
#include <normal/sql/NormalSQLParser.h>
#include <normal/sql/Globals.h>
#include <normal/plan/LogicalPlan.h>
#include <normal/plan/Planner.h>
#include <normal/plan/mode/Modes.h>
#include <normal/cache/LRUCachingPolicy.h>

#include "visitor/Visitor.h"

using namespace normal::sql;

Interpreter::Interpreter() :
  catalogues_(std::make_shared<std::unordered_map<std::string, std::shared_ptr<connector::Catalogue>>>()),
  mode_(normal::plan::operator_::mode::Modes::fullPushdownMode()),
  cachingPolicy_(LRUCachingPolicy::make())
{}

Interpreter::Interpreter(const std::shared_ptr<normal::plan::operator_::mode::Mode> &mode,
                         const std::shared_ptr<CachingPolicy>& cachingPolicy) :
  catalogues_(std::make_shared<std::unordered_map<std::string, std::shared_ptr<connector::Catalogue>>>()),
  mode_(mode),
  cachingPolicy_(cachingPolicy)
{}

void Interpreter::parse(const std::string &sql) {

  SPDLOG_DEBUG("Started");

  std::stringstream ss;
  ss << sql;

  antlr4::ANTLRInputStream input(ss);
  NormalSQLLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  NormalSQLParser parser(&tokens);

  antlr4::tree::ParseTree *tree = parser.parse();
  SPDLOG_DEBUG("Parse Tree:\n{}", tree->toStringTree(true));

  visitor::Visitor visitor(this->catalogues_, this->operatorManager_);
  auto untypedLogicalPlans = tree->accept(&visitor);
  auto logicalPlans = untypedLogicalPlans.as<std::shared_ptr<std::vector<std::shared_ptr<plan::LogicalPlan>>>>();

  // TODO: Perhaps support multiple statements in future
  logicalPlan_ = logicalPlans->at(0);

  // Set mode and queryId
  auto queryId = operatorGraph_->getId();
  for (auto const &logicalOperator: *logicalPlan_->getOperators()) {
    logicalOperator->setMode(mode_);
    logicalOperator->setQueryId(queryId);
  }

  // Create physical plan
  plan::Planner::setQueryId(queryId);
  std::shared_ptr<plan::PhysicalPlan> physicalPlan;
  physicalPlan = plan::Planner::generate(*logicalPlan_, mode_);

  SPDLOG_INFO("Total {} physical operators", physicalPlan->getOperators()->size());
  // Add the plan to the operatorGraph
  for(const auto& physicalOperator: *physicalPlan->getOperators()){
    operatorGraph_->put(physicalOperator.second);
  }

  SPDLOG_DEBUG("Finished");
}

void Interpreter::put(const std::shared_ptr<connector::Catalogue> &catalogue) {
  catalogues_->insert(std::pair(catalogue->getName(), catalogue));
}

const std::shared_ptr<normal::core::OperatorManager> &Interpreter::getOperatorManager() const {
  return operatorManager_;
}

const std::shared_ptr<normal::plan::LogicalPlan> &Interpreter::getLogicalPlan() const {
  return logicalPlan_;
}

void Interpreter::clearOperatorGraph() {
  operatorGraph_.reset();
  operatorGraph_ = graph::OperatorGraph::make(operatorManager_);
}

std::shared_ptr<normal::core::graph::OperatorGraph> &Interpreter::getOperatorGraph() {
  return operatorGraph_;
}

void Interpreter::boot() {
  operatorManager_ = std::make_shared<normal::core::OperatorManager>(cachingPolicy_);
  operatorManager_->boot();
  operatorManager_->start();
}

void Interpreter::stop() {
  operatorManager_->stop();
}

std::string Interpreter::showMetrics() {
  double totalExecutionTime = 0;
  for (auto const executionTime: executionTimes_) {
    totalExecutionTime += executionTime;
  }
  std::stringstream ss;
  ss << std::endl;
  std::stringstream formattedExecutionTime;
  formattedExecutionTime << totalExecutionTime << " secs";
  ss << std::left << std::setw(60) << "Total Execution Time ";
  ss << std::left << std::setw(60) << formattedExecutionTime.str();
  ss << std::endl;

  size_t totalProcessedBytes = 0, totalReturnedBytes = 0;
  for (auto const &bytesTransferredSingle: bytesTransferred_) {
    totalProcessedBytes += bytesTransferredSingle.first;
    totalReturnedBytes += bytesTransferredSingle.second;
  }
  double totalProcessedBytesGiga = ((double)totalProcessedBytes / 1024.0 / 1024.0 / 1024.0);
  double totalReturnedBytesGiga = ((double)totalReturnedBytes / 1024.0 / 1024.0 / 1024.0);
  std::stringstream formattedProcessedBytes;
  formattedProcessedBytes << totalProcessedBytes << " B" << " ("
                          << totalProcessedBytesGiga << " GB)";
  std::stringstream formattedReturnedBytes;
  formattedReturnedBytes << totalReturnedBytes << " B" << " ("
                         << totalReturnedBytesGiga << " GB)";
  size_t totalNumRequests = 0;
  for (auto const &numRequestsSingle: numRequests_) {
    totalNumRequests += numRequestsSingle;
  }

  // Cost of c5a.8xlarge instance in US West (North California)
  // All other costs are for the region US West (North California) as well
  double ec2Price = 1.52, totalCost = 0;
  double getRequestCost = 0.0;
  double s3ScanCost = 0.0;
  double s3ReturnCost = 0.0;
  double runtimeCost = 0.0;
  if (mode_->id() == normal::plan::operator_::mode::ModeId::FullPullup ||
      mode_->id() == normal::plan::operator_::mode::ModeId::PullupCaching) {
    getRequestCost = ((double) totalNumRequests) * 0.00000044;  // GET request cost
    runtimeCost = totalExecutionTime / 3600 * ec2Price;         // runtime cost
    totalCost = getRequestCost + runtimeCost;
  } else {
    getRequestCost = ((double) totalNumRequests) * 0.00000044;  // GET request cost
    s3ScanCost = totalProcessedBytesGiga * 0.0022;              // s3 scan cost
    s3ReturnCost = totalReturnedBytesGiga * 0.0008;             // s3 return cost
    runtimeCost = totalExecutionTime / 3600 * ec2Price;         // runtime cost
    totalCost = getRequestCost + s3ScanCost + s3ReturnCost + runtimeCost;
  }
  std::stringstream formattedCost;
  formattedCost << totalCost << " $";
  std::stringstream formattedGetRequestCost;
  formattedGetRequestCost << getRequestCost << " $";
  std::stringstream formattedS3ScanCost;
  formattedS3ScanCost << s3ScanCost << " $";
  std::stringstream formattedS3ReturnCost;
  formattedS3ReturnCost << s3ReturnCost << " $";
  std::stringstream formattedRuntimeCost;
  formattedRuntimeCost << runtimeCost << " $";

  ss << std::left << std::setw(60) << "Total Processed Bytes";
  ss << std::left << std::setw(60) << formattedProcessedBytes.str();
  ss << std::endl;
  ss << std::left << std::setw(60) << "Total Returned Bytes";
  ss << std::left << std::setw(60) << formattedReturnedBytes.str();
  ss << std::endl;
  ss << std::left << std::setw(60) << "Total Request amount";
  ss << std::left << std::setw(60) << totalNumRequests;
  ss << std::endl;
  ss << std::left << std::setw(60) << "Total Cost";
  ss << std::left << std::setw(60) << formattedCost.str();
  ss << std::endl;
  ss << std::left << std::setw(60) << "Total GET Request Cost";
  ss << std::left << std::setw(60) << formattedGetRequestCost.str();
  ss << std::endl;
  ss << std::left << std::setw(60) << "Total S3 Scan Cost";
  ss << std::left << std::setw(60) << formattedS3ScanCost.str();
  ss << std::endl;
  ss << std::left << std::setw(60) << "Total S3 Return Cost";
  ss << std::left << std::setw(60) << formattedS3ReturnCost.str();
  ss << std::endl;
  ss << std::left << std::setw(60) << "Total Runtime Cost";
  ss << std::left << std::setw(60) << formattedRuntimeCost.str();
  ss << std::endl;
  ss << std::endl;

  ss << std::left << std::setw(120) << "Query Execution Times and Bytes Transferred" << std::endl;
  ss << std::setfill(' ');
  ss << std::left << std::setw(120) << std::setfill('-') << "" << std::endl;
  ss << std::setfill(' ');
  ss << std::left << std::setw(15) << "Query";
  ss << std::left << std::setw(30) << "Execution Time";
  ss << std::left << std::setw(30) << "Processed Bytes";
  ss << std::left << std::setw(30) << "Returned Bytes";
  ss << std::endl;
  ss << std::left << std::setw(120) << std::setfill('-') << "" << std::endl;
  ss << std::setfill(' ');
  for (int qid = 1; qid <= executionTimes_.size(); ++qid) {
    std::stringstream formattedProcessingTime1;
    formattedProcessingTime1 << executionTimes_[qid - 1] << " secs";
    std::stringstream formattedProcessedBytes1;
    formattedProcessedBytes1 << bytesTransferred_[qid - 1].first << " B" << " ("
                             << ((double)bytesTransferred_[qid - 1].first / 1024.0 / 1024.0 / 1024.0) << " GB)";
    std::stringstream formattedReturnedBytes1;
    formattedReturnedBytes1 << bytesTransferred_[qid - 1].second << " B" << " ("
                            << ((double)bytesTransferred_[qid - 1].second / 1024.0 / 1024.0 / 1024.0) << " GB)";
    ss << std::left << std::setw(15) << std::to_string(qid);
    ss << std::left << std::setw(30) << formattedProcessingTime1.str();
    ss << std::left << std::setw(30) << formattedProcessedBytes1.str();
    ss << std::left << std::setw(30) << formattedReturnedBytes1.str();
    ss << std::endl;
  }

  return ss.str();
}

void Interpreter::saveMetrics() {
  executionTimes_.emplace_back((double) (operatorGraph_->getElapsedTime().value()) / 1000000000.0);
  bytesTransferred_.emplace_back(operatorGraph_->getBytesTransferred());
  numRequests_.emplace_back(operatorGraph_->getNumRequests());
}

void Interpreter::clearMetrics() {
  executionTimes_.clear();
  bytesTransferred_.clear();
  numRequests_.clear();
}

const std::shared_ptr<CachingPolicy> &Interpreter::getCachingPolicy() const {
  return cachingPolicy_;
}

std::string Interpreter::showHitRatios() {
  std::stringstream ss;
  ss << std::endl;
  ss << "Hit ratios, Shard Hit Ratios:" << std::endl;
  ss << std::endl;
  for (int i = 0; i < hitRatios_.size(); i++) {
    int qId = i + 1;
    auto const hitRatio = hitRatios_.at(i);
    auto const shardHitRatio = shardHitRatios_.at(i);
    ss << std::left << std::setw(20) << qId;
    ss << std::left << std::setw(30) << hitRatio;
    ss << std::left << std::setw(30) << shardHitRatio;
    ss << std::endl;
  }
  return ss.str();
}

void Interpreter::saveHitRatios() {
  hitRatios_.emplace_back(operatorManager_->getCrtQueryHitRatio());
  shardHitRatios_.emplace_back(operatorManager_->getCrtQueryShardHitRatio());
}


