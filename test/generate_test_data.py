import pandas as pd
import pyarrow as pa
import pyarrow.parquet as pq

df = pd.DataFrame({
    'id': [1, 2, 3],
})

table = pa.Table.from_pandas(df)
pq.write_table(table, 'mysql-test/my-quiver/count_star.parquet', row_group_size=1)


df = pd.DataFrame({
    'c1': [1, 2, 3],
    'c2': [10, 20, 30],
    'c3': [100, 200, 300],
})

table = pa.Table.from_pandas(df)
pq.write_table(table, 'mysql-test/my-quiver/select_all.parquet')


df = pd.DataFrame({
    'int8_t': [-2**7, 2**7-1],
    'uint8_t': [0, 2**8-1],
    'int16_t': [-2**15, 2**15-1],
    'uint16_t': [0, 2**16-1],
    'int24_t': [-2**23, 2**23-1],
    'uint24_t': [0, 2**24-1],
    'int32_t': [-2**31, 2**31-1],
    'uint32_t': [0, 2**32-1],
    'int64_t': [-2**63, 2**63-1],
    'uint64_t': [0, 2**64-1],
    'float_t': [1.175494e-38, 3.402823e38],
    'double_t': [2.225074e-308, 1.797693e308],
    'varchar_t': ['', 'varchar'],
    'text_t': ['', 'text'],
})

table = pa.Table.from_pandas(df)
pq.write_table(table, 'mysql-test/my-quiver/select_types.parquet')
