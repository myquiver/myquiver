#include "ha_my_quiver.hpp"

ha_my_quiver::ha_my_quiver(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg) {} // TODO: Initialize instance val


static struct st_mysql_storage_engine my_quiver_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION
};

static handler *my_quiver_create_handler(handlerton *hton, TABLE_SHARE *table,
                                       bool, MEM_ROOT *mem_root) {
  return new (mem_root) ha_my_quiver(hton, table);
}

static int my_quiver_init_func(void *p) {
  DBUG_TRACE;

  auto hton = static_cast<handlerton *>(p);
  hton->state = SHOW_OPTION_YES;
  hton->create = my_quiver_create_handler;
  hton->flags = 
    (HTON_ALTER_NOT_SUPPORTED | HTON_CAN_RECREATE | HTON_NO_PARTITION);

  arrow::dataset::internal::Initialize();

  return 0;
}

mysql_declare_plugin(my_quiver){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &my_quiver_storage_engine,
    "MyQuiver",
    "Shoichi Kagawa",
    "MySQL storage engine powered by Apache Arrow",
    PLUGIN_LICENSE_BSD,
    my_quiver_init_func, /* Plugin Init */
    nullptr,           /* Plugin check uninstall */
    nullptr,           /* Plugin Deinit */
    0x0001 /* 0.1 */,
    nullptr,              /* status variables */
    nullptr, /* system variables */
    nullptr,                  /* config options */
    0,                        /* flags */
} mysql_declare_plugin_end;
