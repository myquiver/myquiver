#include <sql/handler.h>
#include <mysql/plugin.h>

class ha_my_quiver : public handler {
  public:
    ha_my_quiver(handlerton *hton, TABLE_SHARE *table_arg);
    ~ha_my_quiver() override = default;
  
  int rnd_init(bool scan) override {
    DBUG_TRACE;
    return 0;
  }
  int rnd_end() override {
    DBUG_TRACE;
    return 0;
  }
  int rnd_next(uchar *buf) override {
    DBUG_TRACE;
    return HA_ERR_END_OF_FILE;
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
  int open(const char *, int, uint, const dd::Table *) override {
    DBUG_TRACE;
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
};
