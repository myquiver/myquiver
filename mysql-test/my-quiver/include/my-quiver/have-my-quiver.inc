disable_query_log;

let original_default_storage_engine=`SELECT @@global.default_storage_engine`;
set default_storage_engine=MyQuiver;

let original_default_tmp_storage_engine=`SELECT @@global.default_tmp_storage_engine`;
if (!$original_default_tmp_storage_engine) {
    let original_default_tmp_storage_engine=NULL;
}
set default_tmp_storage_engine=MyQuiver;

enable_query_log;
