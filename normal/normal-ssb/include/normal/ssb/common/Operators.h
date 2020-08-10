//
// Created by matt on 10/8/20.
//

#ifndef NORMAL_NORMAL_SSB_INCLUDE_NORMAL_SSB_COMMON_OPERATORS_H
#define NORMAL_NORMAL_SSB_INCLUDE_NORMAL_SSB_COMMON_OPERATORS_H


#include <vector>
#include <memory>

#include <normal/pushdown/AWSClient.h>
#include <normal/pushdown/file/FileScan.h>
#include <normal/pushdown/filter/Filter.h>
#include <normal/pushdown/Collate.h>
#include <normal/pushdown/join/HashJoinBuild.h>
#include <normal/pushdown/join/HashJoinProbe.h>
#include <normal/pushdown/Aggregate.h>
#include <normal/pushdown/s3/S3SelectScan.h>
#include <normal/pushdown/shuffle/Shuffle.h>
#include <normal/core/graph/OperatorGraph.h>
#include <normal/pushdown/cache/CacheLoad.h>
#include <normal/pushdown/merge/MergeOperator.h>
#include <normal/pushdown/bloomjoin/BloomCreateOperator.h>
#include <normal/pushdown/bloomjoin/FileScanBloomUseOperator.h>

using namespace normal::core::graph;
using namespace normal::pushdown;
using namespace normal::pushdown::filter;
using namespace normal::pushdown::join;
using namespace normal::pushdown::shuffle;
using namespace normal::pushdown::cache;
using namespace normal::pushdown::merge;
using namespace normal::pushdown::bloomjoin;

namespace normal::ssb::common {

class Operators {

public:

  static std::vector<std::shared_ptr<CacheLoad>>
  makeFileCacheLoadOperators(const std::string &namePrefix,
							 const std::string &filename,
							 const std::vector<std::string> &columns,
							 const std::string &dataDir,
							 int numConcurrentUnits,
							 const std::shared_ptr<OperatorGraph> &g);

  static std::vector<std::shared_ptr<FileScan>>
  makeFileScanOperators(const std::string &namePrefix,
						const std::string &filename,
						const std::vector<std::string> &columns,
						const std::string &dataDir,
						int numConcurrentUnits,
						const std::shared_ptr<OperatorGraph> &g);

  static std::vector<std::shared_ptr<MergeOperator>>
  makeMergeOperators(const std::string &namePrefix,
					 int numConcurrentUnits,
					 const std::shared_ptr<OperatorGraph> &g);

  static std::vector<std::shared_ptr<Shuffle>>
  makeShuffleOperators(const std::string &namePrefix,
					   const std::string &columnName,
					   int numConcurrentUnits,
					   const std::shared_ptr<OperatorGraph> &g);

  static std::vector<std::shared_ptr<HashJoinBuild>>
  makeHashJoinBuildOperators(const std::string &namePrefix,
							 const std::string &columnName,
							 int numConcurrentUnits,
							 const std::shared_ptr<OperatorGraph> &g);

  static std::vector<std::shared_ptr<HashJoinProbe>>
  makeHashJoinProbeOperators(const std::string &namePrefix,
							 const std::string &leftColumnName,
							 const std::string &rightColumnName,
							 int numConcurrentUnits,
							 const std::shared_ptr<OperatorGraph> &g);

  static std::shared_ptr<Collate> makeCollateOperator(const std::shared_ptr<OperatorGraph> &g);

  template <typename A, typename B>
  static void connectToEach(std::vector<std::shared_ptr<A>> producers,
							std::vector<std::shared_ptr<B>> consumers){
	assert(producers.size() == consumers.size());

	for (size_t u = 0; u < producers.size(); ++u) {
	  producers[u]->produce(consumers[u]);
	  consumers[u]->consume(producers[u]);
	}
  }

  template <typename A, typename B>
  static void connectToAll(std::vector<std::shared_ptr<A>> producers,
						   std::vector<std::shared_ptr<B>> consumers){

	for (size_t u1 = 0; u1 < producers.size(); ++u1) {
	  for (size_t u2 = 0; u2 < consumers.size(); ++u2) {
		producers[u1]->produce(consumers[u2]);
		consumers[u2]->consume(producers[u1]);
	  }
	}
  }

  template <typename A, typename B>
  static void connectToOne(std::vector<std::shared_ptr<A>> producers,
						   std::shared_ptr<B> consumer){

	for (size_t u1 = 0; u1 < producers.size(); ++u1) {
	  producers[u1]->produce(consumer);
	  consumer->consume(producers[u1]);
	}
  }


  template <typename B>
  static void connectHitsToEach(std::vector<std::shared_ptr<CacheLoad>> producers,
								std::vector<std::shared_ptr<B>> consumers){

	for (size_t u1 = 0; u1 < producers.size(); ++u1) {
	  producers[u1]->setHitOperator(consumers[u1]);
	  consumers[u1]->consume(producers[u1]);
	}
  }


  template <typename B>
  static void connectMissesToEach(std::vector<std::shared_ptr<CacheLoad>> producers,
								  std::vector<std::shared_ptr<B>> consumers){

	for (size_t u1 = 0; u1 < producers.size(); ++u1) {
	  producers[u1]->setMissOperator(consumers[u1]);
	  consumers[u1]->consume(producers[u1]);
	}
  }

};

}


#endif //NORMAL_NORMAL_SSB_INCLUDE_NORMAL_SSB_COMMON_OPERATORS_H
