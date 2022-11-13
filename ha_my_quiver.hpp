#define MYSQL_DYNAMIC_PLUGIN

#include <sql/handler.h>
#include <sql/table.h>
#include <sql/field.h>
#include <mysql/plugin.h>

#undef base_name

#include <parquet/arrow/reader.h>
#include <arrow/io/api.h>
#include <arrow/record_batch.h>
#include <arrow/array.h>
#include <arrow/compute/api.h>
#include <arrow/compute/api_vector.h>
#include <arrow/compute/cast.h>
#include <arrow/compute/exec/exec_plan.h>
#include <arrow/dataset/dataset.h>
#include <arrow/dataset/file_base.h>
#include <arrow/dataset/file_parquet.h>
#include <arrow/dataset/plan.h>
#include <arrow/dataset/scanner.h>
#include <arrow/util/thread_pool.h>

namespace mqv {
  class DebugColumnAccess {
    TABLE *table_;
    MY_BITMAP *bitmap_;
#ifndef DBUG_OFF
    my_bitmap_map *map_;
#endif
  public:
    DebugColumnAccess(TABLE *table, MY_BITMAP *bitmap): table_(table), bitmap_(bitmap) {
#ifndef DBUG_OFF
      map_ = dbug_tmp_use_all_columns(table_, bitmap_);
#endif
    };
    ~DebugColumnAccess() {
#ifndef DBUG_OFF
      dbug_tmp_restore_column_map(bitmap_, map_);
#endif
    };
  };
};

class ha_my_quiver : public handler {
  public:
    ha_my_quiver(handlerton *hton, TABLE_SHARE *table_arg);
    ~ha_my_quiver() override = default;
  
  int rnd_init(bool scan) override {
    DBUG_TRACE;

    exec_context_ = std::make_unique<arrow::compute::ExecContext>();
    auto plan_status = [&]() -> arrow::Status {
      ARROW_ASSIGN_OR_RAISE(exec_plan_, arrow::compute::ExecPlan::Make(exec_context_.get()));

      ARROW_ASSIGN_OR_RAISE(auto dataset, dateset_factory_->Finish());
      auto schema = dataset->schema();
      auto scan_options = std::make_shared<arrow::dataset::ScanOptions>();
      scan_options->projection = arrow::compute::project({}, {});  // create empty projection
      auto scan_node_options = arrow::dataset::ScanNodeOptions{dataset, scan_options};
      ARROW_ASSIGN_OR_RAISE(auto scan, arrow::compute::MakeExecNode("scan", exec_plan_.get(), {}, scan_node_options));

      // TODO: Change to std:optional after updating arrow
      arrow::AsyncGenerator<arrow::util::optional<arrow::compute::ExecBatch>> sink_gen;
      ARROW_RETURN_NOT_OK(arrow::compute::MakeExecNode("sink", exec_plan_.get(), {scan}, arrow::compute::SinkNodeOptions{&sink_gen}));
      record_batch_reader_ = arrow::compute::MakeGeneratorReader(schema, std::move(sink_gen), exec_context_->memory_pool());
      ARROW_RETURN_NOT_OK(exec_plan_->StartProducing());
      
      return arrow::Status::OK();
    }();

    if (!plan_status.ok()) {
      return 4; // TODO: return error code
    }

    auto record_batch_result = record_batch_reader_->Next();
    if (!record_batch_result.ok()) {
      return 2; // TODO: return error code
    }
    record_batch_ = *record_batch_result;
    nth_row_ = 0;
    return 0;
  }
  int rnd_end() override {
    DBUG_TRACE;
    record_batch_reader_ = nullptr;
    exec_plan_->StopProducing();
    auto future = exec_plan_->finished();
    auto status = future.status();
    if (!status.ok()) {
      return 3; // TODO: return error code
    }
    return 0;
  }
  int rnd_next(uchar *buf) override {
    DBUG_TRACE;
    auto column = std::static_pointer_cast<arrow::Int64Array>(record_batch_->columns()[0]);
    auto value = column->Value(nth_row_);
    auto field = static_cast<Field_longlong*>(table->field[0]);
    {
      mqv::DebugColumnAccess debug_column_access(table, table->write_set);
      field->set_notnull();
      field->store(value, false);
    }
    if (nth_row_ >= record_batch_->num_rows()) {
      //TODO: record_batch_readerをNextする
      return HA_ERR_END_OF_FILE;
    }
    nth_row_++;
    return 0;
  }
  int rnd_pos(uchar *buf, uchar *pos) override {
    DBUG_TRACE;
    return HA_ERR_WRONG_COMMAND;
  }
  void position(const uchar *) override { DBUG_TRACE; }
  int info(uint) override {
    DBUG_TRACE;
    return 0;
  }
  const char *table_type() const override { return "MyQuiver"; }
  ulong index_flags(uint inx [[maybe_unused]], uint part [[maybe_unused]],
                    bool all_parts [[maybe_unused]]) const override {
    return 0;
  }
  THR_LOCK_DATA **store_lock(THD *, THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type) override {
    return to;
  }
  int open(const char *name, int, uint, const dd::Table *) override {
    DBUG_TRACE;

    auto status = [&]() -> arrow::Status {
      // TODO: Move this initialize to more suitable position. 
      arrow::dataset::internal::Initialize();

      char setup_path[256];
      char* result = getcwd(setup_path, 256);
      if (result == NULL) {
        return arrow::Status::IOError("Fetching PWD failed.");
      }
      // TODO: Fix file location to support both local file path and remote file path.
      ARROW_ASSIGN_OR_RAISE(auto fs, arrow::fs::FileSystemFromUriOrPath(setup_path));
      arrow::fs::FileSelector selector;
      selector.recursive = true;
      selector.base_dir = "test";
      arrow::dataset::FileSystemFactoryOptions options;
      options.exclude_invalid_files = true; // TODO: There is invalid parquet file in the test directories. 
      auto read_format = std::make_shared<arrow::dataset::ParquetFileFormat>();
      ARROW_ASSIGN_OR_RAISE(dateset_factory_, arrow::dataset::FileSystemDatasetFactory::Make(fs, selector, read_format, options));

      return arrow::Status::OK();
    }();

    if (!status.ok()) {
      return 1; // TODO: return error code
    }

    return 0;
  }
  int close(void) override {
    DBUG_TRACE;
    return 0;
  }
  ulonglong table_flags() const override {
    return HA_BINLOG_STMT_CAPABLE;
  }
  int create(const char *name, TABLE *, HA_CREATE_INFO *,
                       dd::Table *) override {
    DBUG_TRACE;

    return 0;
  }

  private:
    std::shared_ptr<arrow::dataset::DatasetFactory> dateset_factory_;
    std::shared_ptr<arrow::RecordBatchReader> record_batch_reader_;
    std::shared_ptr<arrow::RecordBatch> record_batch_;
    std::unique_ptr<arrow::compute::ExecContext> exec_context_;
    std::shared_ptr<arrow::compute::ExecPlan> exec_plan_;
    int64_t nth_row_;
};
