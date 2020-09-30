//
// Created by matt on 23/9/20.
//

#ifndef NORMAL_NORMAL_PUSHDOWN_INCLUDE_NORMAL_PUSHDOWN_FILE_FILESCAN2_H
#define NORMAL_NORMAL_PUSHDOWN_INCLUDE_NORMAL_PUSHDOWN_FILE_FILESCAN2_H

#include <caf/all.hpp>

#include <normal/core/Forward.h>
#include <normal/core/OperatorActor2.h>
#include <normal/core/cache/SegmentCacheActor.h>
#include <normal/core/cache/StoreRequestMessage.h>
#include <normal/connector/local-fs/LocalFilePartition.h>

#include <normal/pushdown/Forward.h>
#include <normal/pushdown/ScanOperator.h>
#include <normal/pushdown/file/FileScan.h>
#include <normal/pushdown/TupleMessage.h>

using namespace normal::core;
using namespace normal::core::message;
using namespace normal::core::cache;
using namespace normal::tuple;

namespace normal::pushdown {

using FileScanActor = OperatorActor2::extend_with<::caf::typed_actor<
	caf::reacts_to<ScanAtom, std::vector<std::string>, bool>,
	caf::reacts_to<Envelope>>>;

using FileScanStatefulActor = FileScanActor::stateful_pointer<FileScanState>;

class FileScanState : public OperatorActorState<FileScanStatefulActor> {
public:
  void setState(FileScanStatefulActor actor,
				const char *name,
				const std::string &filePath,
				FileType fileType,
				const std::vector<std::string> &columnNames,
				unsigned long startOffset,
				unsigned long finishOffset,
				long queryId,
				const caf::actor &rootActorHandle,
				const caf::actor &segmentCacheActorHandle,
				bool scanOnStart = false) {

	OperatorActorState::setBaseState(actor, name, rootActorHandle, segmentCacheActorHandle);

	filePath_ = filePath;
	fileType_ = fileType;
	columnNames_ = columnNames;
	startOffset_ = startOffset;
	finishOffset_ = finishOffset;
	queryId_ = queryId;
	scanOnStart_ = scanOnStart;

	kernel_ = FileScanKernel::make(filePath, fileType, startOffset, finishOffset);
  }

  template<class... Handlers>
  FileScanActor::behavior_type makeBehavior(FileScanStatefulActor actor, Handlers... handlers) {
	return OperatorActorState::makeBaseBehavior(
		actor,
		[=](ScanAtom, const std::vector<std::string> &columnNames, bool resultNeeded) {
		  SPDLOG_DEBUG("[Actor {} ('{}')]  Scan  |  sender: {}", actor->id(),
					   actor->name(), to_string(actor->current_sender()));
		  process(actor, [=](auto /*messageSender*/) { onScan(actor, columnNames); });
		},
		[=](const Envelope &envelope) {
		  SPDLOG_DEBUG("[Actor {} ('{}')]  Envelope  |  sender: {}", actor->id(),
					   actor->name(), to_string(actor->current_sender()));
		  process(actor, [=](auto /*messageSender*/) { onReceive(actor, envelope); });
		},
		std::move(handlers)...
	);
  }

private:
  std::string filePath_;
  FileType fileType_;
  std::vector<std::string> columnNames_;
  unsigned long startOffset_;
  unsigned long finishOffset_;
  long queryId_;
  bool scanOnStart_ = false;

  std::unique_ptr<FileScanKernel> kernel_;

protected:

  void onStart(FileScanStatefulActor actor, const caf::strong_actor_ptr& /*messageSender*/) override {
	if (scanOnStart_) {
	  readAndSendTuples(actor, columnNames_);
	  notifyComplete(actor);
	}
  }

  void onComplete(FileScanStatefulActor actor, const caf::strong_actor_ptr& /*messageSender*/) override {
	if (isAllProducersComplete()) {
	  notifyComplete(actor);
	}
  }

  void onEnvelope(FileScanStatefulActor actor, const caf::strong_actor_ptr& /*messageSender*/, const Envelope &envelope) override {
	onReceive(actor, envelope);
  }

private:
  void onReceive(FileScanStatefulActor actor, const Envelope &message) {
	if (message.message().type() == "ScanMessage") {
	  auto scanMessage = dynamic_cast<const ScanMessage &>(message.message());
	  this->onScan(actor, scanMessage.getColumnNames());
	} else {
	  // FIXME: Propagate error properly
	  throw std::runtime_error("Unrecognized message type " + message.message().type());
	}
  }

  void onScan(FileScanStatefulActor actor, const std::vector<std::string> &columnsToScan) {
	readAndSendTuples(actor, columnsToScan);
  }

  void requestStoreSegmentsInCache(FileScanStatefulActor actor, const std::shared_ptr<TupleSet2> &tupleSet) {

	assert(tupleSet);
	assert(startOffset_ >= 0);
	assert(finishOffset_ > startOffset_);

	auto partition = std::make_shared<LocalFilePartition>(kernel_->getPath());

	std::unordered_map<std::shared_ptr<SegmentKey>, std::shared_ptr<SegmentData>> segmentsToStore;
	for (int64_t c = 0; c < tupleSet->numColumns(); ++c) {
	  auto column = tupleSet->getColumnByIndex(c).value();
	  auto segmentKey = SegmentKey::make(partition,
										 column->getName(),
										 SegmentRange::make(startOffset_, finishOffset_),
										 SegmentMetadata::make(0, column->size()));
	  auto segmentData = SegmentData::make(column);

	  segmentsToStore.emplace(segmentKey, segmentData);
	}

	anonymousSend(actor,
				  getSegmentCacheActorHandle(),
				  StoreAtom::value,
				  StoreRequestMessage::make(segmentsToStore, name));
  }

  void readAndSendTuples(FileScanStatefulActor actor, const std::vector<std::string> &columnNames) {
	// Read the columns not present in the cache
	/*
	 * FIXME: Should support reading the file in pieces
	 */

	std::shared_ptr<TupleSet2> readTupleSet;
	if (columnNames.empty()) {
	  readTupleSet = TupleSet2::make2();
	} else {
	  auto expectedReadTupleSet = kernel_->scan(columnNames);
	  readTupleSet = expectedReadTupleSet.value();

	  // Store the read columns in the cache
	  requestStoreSegmentsInCache(actor, readTupleSet);
	}

	std::shared_ptr<Message> message = std::make_shared<TupleMessage>(readTupleSet->toTupleSetV1(), this->name);
	tell(actor, Envelope(message));
  }

};

FileScanActor::behavior_type FileScanFunctor(FileScanStatefulActor actor,
											 const char *name,
											 const std::string &filePath,
											 FileType fileType,
											 const std::vector<std::string> &columnNames,
											 unsigned long startOffset,
											 unsigned long finishOffset,
											 long queryId,
											 const caf::actor &rootActorHandle,
											 const caf::actor &segmentCacheActorHandle,
											 bool scanOnStart = false);

}

#endif //NORMAL_NORMAL_PUSHDOWN_INCLUDE_NORMAL_PUSHDOWN_FILE_FILESCAN2_H