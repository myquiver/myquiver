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

#include <filesystem>

#define ERR_MYQUIVER_FIRST 600
#define ERR_MYQUIVER_FROM_ARROW_NUM ERR_MYQUIVER_FIRST
#define ERR_MYQUIVER_FROM_ARROW_STR "Error from arrow [%s]"

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

static int arrow_status_error(arrow::Status status) {
  auto message = status.message();
  my_printf_error(ERR_MYQUIVER_FROM_ARROW_NUM, ERR_MYQUIVER_FROM_ARROW_STR, MYF(0), message.c_str());
  return ERR_MYQUIVER_FROM_ARROW_NUM;
}

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
      arrow::AsyncGenerator<std::optional<arrow::compute::ExecBatch>> sink_gen;

      ARROW_ASSIGN_OR_RAISE(
        auto declaration, arrow::compute::Declaration::Sequence({
          {"scan", arrow::dataset::ScanNodeOptions{dataset, scan_options}},
          {"sink", arrow::compute::SinkNodeOptions{&sink_gen}},
        }).AddToPlan(exec_plan_.get()));

      ARROW_RETURN_NOT_OK(declaration->Validate());
      ARROW_RETURN_NOT_OK(exec_plan_->Validate());

      ARROW_RETURN_NOT_OK(exec_plan_->StartProducing());  
      record_batch_reader_ = arrow::compute::MakeGeneratorReader(schema, std::move(sink_gen), exec_context_->memory_pool());

      return arrow::Status::OK();
    }();

    if (!plan_status.ok()) {
      return arrow_status_error(plan_status);
    }

    auto record_batch_result = record_batch_reader_->Next();
    if (!record_batch_result.ok()) {
      auto status = record_batch_result.status();
      return arrow_status_error(status);
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
      return arrow_status_error(status);
    }
    return 0;
  }

  void store_field_integer(Field *field, std::shared_ptr<arrow::Array> array) {
    auto casted_array = std::static_pointer_cast<arrow::Int64Array>(array);
    auto value = casted_array->Value(nth_row_);
    {
      mqv::DebugColumnAccess debug_column_access(table, table->write_set);
      field->store(value, field->is_unsigned());
    }
  }

  void store_field_floating_point(Field *field, std::shared_ptr<arrow::Array> array) {
    auto casted_array = std::static_pointer_cast<arrow::DoubleArray>(array);
    auto value = casted_array->Value(nth_row_);
    {
      mqv::DebugColumnAccess debug_column_access(table, table->write_set);
      field->store(value);
    }
  }

  void store_field_string(Field *field, std::shared_ptr<arrow::Array> array) {
    auto casted_array = std::static_pointer_cast<arrow::StringArray>(array);
    auto value = casted_array->Value(nth_row_);
    auto str = std::string(value).c_str();
    {
      mqv::DebugColumnAccess debug_column_access(table, table->write_set);
      //TODO: Need to care if charset is not utf8mb4
      field->store(str, value.length(), field->charset());
    }
  }

  void store_field(Field *field, std::shared_ptr<arrow::Array> array) {
    // TODO: Need to handle overflow and to cause error when unsupported type is passed.
    if (!array) {
      return;
    }
    field->set_notnull();
    switch (field->real_type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONGLONG:
      store_field_integer(field, array);
      break;
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
      store_field_floating_point(field, array);
      break;
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_BLOB:
      // TODO: Need to support blob type
      store_field_string(field, array);
      break;
    }
  }

  int rnd_next(uchar *buf) override {
    DBUG_TRACE;
    if (!record_batch_) {
      return HA_ERR_END_OF_FILE;
    }

    auto n_columns = table->s->fields;
    for (auto i = 0; i < n_columns; i++) {
      auto field = table->field[i];
      auto name = field->field_name;
      auto column = record_batch_->GetColumnByName(name);
      store_field(field, column);
    }

    nth_row_++;
    if (nth_row_ >= record_batch_->num_rows()) {
      auto record_batch_result = record_batch_reader_->Next();
      if (!record_batch_result.ok()) {
        auto status = record_batch_result.status();
        return arrow_status_error(status);
      }
      record_batch_ = *record_batch_result;
      nth_row_ = 0;
    }
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

      auto data_path = std::filesystem::current_path();
      data_path /= std::string(name)+".parquet";
      auto uri = "file://" + data_path.string();
      arrow::dataset::FileSystemFactoryOptions options;
      options.exclude_invalid_files = true; // TODO: There is invalid parquet file in the test directories. 
      auto read_format = std::make_shared<arrow::dataset::ParquetFileFormat>();
      ARROW_ASSIGN_OR_RAISE(dateset_factory_, arrow::dataset::FileSystemDatasetFactory::Make(uri, read_format, options));

      return arrow::Status::OK();
    }();

    if (!status.ok()) {
      return arrow_status_error(status);
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
