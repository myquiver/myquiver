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
