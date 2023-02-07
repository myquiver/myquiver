import os
from dataclasses import dataclass

import pandas as pd
import pyarrow as pa
import pyarrow.parquet as pq

test_dir = os.path.join(os.path.dirname(__file__), "../mysql-test/my-quiver/")

@dataclass
class Dataset:
    file_name: str
    data: dict
    options: dict = None

datasets = [
    Dataset('count_star.parquet', {'id': [1, 2, 3]}, {'row_group_size': 1}),
    Dataset('select_all.parquet', {'c1': [1, 2, 3], 'c2': [10, 20, 30], 'c3': [100, 200, 300]}),
    Dataset('select_int8.parquet', {'int8_t': [-2**7, 2**7-1]}),
    Dataset('select_uint8.parquet', {'uint8_t': [0, 2**8-1]}),
    Dataset('select_int16.parquet', {'int16_t': [-2**15, 2**15-1]}),
    Dataset('select_uint16.parquet', {'uint16_t': [0, 2**16-1]}),
    Dataset('select_int24.parquet', {'int24_t': [-2**23, 2**23-1]}),
    Dataset('select_uint24.parquet', {'uint24_t': [0, 2**24-1]}),
    Dataset('select_int32.parquet', {'int32_t': [-2**31, 2**31-1]}),
    Dataset('select_uint32.parquet', {'uint32_t': [0, 2**32-1]}),
    Dataset('select_int64.parquet', {'int64_t': [-2**63, 2**63-1]}),
    Dataset('select_uint64.parquet', {'uint64_t': [0, 2**64-1]}),
    Dataset('select_float.parquet', {'float_t': [1.175494e-38, 3.402823e38]}),
    Dataset('select_double.parquet', {'double_t': [2.225074e-308, 1.797693e308]}),
    Dataset('select_varchar.parquet', {'varchar_t': ['', 'varchar']}),
    Dataset('select_text.parquet', {'text_t': ['', 'text']}),
]

for dataset in datasets:
    df = pd.DataFrame(dataset.data)
    table = pa.Table.from_pandas(df)

    
    kwargs = {'table': table, 'where': os.path.join(test_dir, dataset.file_name)}
    if dataset.options:
        kwargs |= dataset.options
    pq.write_table(**kwargs)
