#define MYSQL_DYNAMIC_PLUGIN

#include <sql/handler.h>
#include <mysql/plugin.h>
#include <parquet/arrow/reader.h>
#include <arrow/io/api.h>
#include <arrow/record_batch.h>

class ha_my_quiver : public handler {
  public:
    ha_my_quiver(handlerton *hton, TABLE_SHARE *table_arg);
    ~ha_my_quiver() override = default;
  
  int rnd_init(bool scan) override {
    DBUG_TRACE;
    auto num_row_groups = reader_->num_row_groups();
    std::vector<int> row_group_indices(num_row_groups);
    std::iota(row_group_indices.begin(), row_group_indices.end(), 0);
    auto status = reader_->GetRecordBatchReader(row_group_indices, &record_batch_reader_);
    if (!status.ok()) {
      return 1; // TODO: return error code
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
    return 0;
  }
  int rnd_next(uchar *buf) override {
    DBUG_TRACE;
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

    auto input_result = arrow::io::ReadableFile::Open(std::string(name)+".parquet");
    if (!input_result.ok()) {
      return 1; // TODO: return error code
    }
    auto status = parquet::arrow::OpenFile(*input_result, arrow::default_memory_pool(), &reader_);
    if (!status.ok()) {
      return 2; // TODO: return error code
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
    auto input_result = arrow::io::ReadableFile::Open(std::string(name)+".parquet");
    if (!input_result.ok()) {
      return 1; // TODO: return error code
    }
    auto status = parquet::arrow::OpenFile(*input_result, arrow::default_memory_pool(), &reader_);
    if (!status.ok()) {
      return 2; // TODO: return error code
    }

    return 0;
  }

  private:
    std::unique_ptr<parquet::arrow::FileReader> reader_;
    std::unique_ptr<arrow::RecordBatchReader> record_batch_reader_;
    std::shared_ptr<arrow::RecordBatch> record_batch_;
    int64_t nth_row_;
};
