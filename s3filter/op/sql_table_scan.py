# -*- coding: utf-8 -*-
"""

"""
import cProfile
import time

from boto3 import Session
from botocore.config import Config

from s3filter.op.message import TupleMessage, StringMessage
from s3filter.op.operator_base import Operator, StartMessage
from s3filter.op.tuple import Tuple, IndexedTuple
from s3filter.plan.op_metrics import OpMetrics
from s3filter.sql.cursor import Cursor
from s3filter.util.constants import *
# noinspection PyCompatibility,PyPep8Naming
import cPickle as pickle

from s3filter.sql.pandas_cursor import PandasCursor
import pandas as pd

class SQLTableScanMetrics(OpMetrics):
    """Extra metrics for a sql table scan

    """

    # These amounts vary by region, but let's assume it's a flat rate for simplicity
    COST_S3_DATA_RETURNED_PER_GB = 0.0007
    COST_S3_DATA_SCANNED_PER_GB = 0.002

    def __init__(self):
        super(SQLTableScanMetrics, self).__init__()

        self.rows_returned = 0

        self.time_to_first_response = 0
        self.time_to_first_record_response = None
        self.time_to_last_record_response = None

        self.query_bytes = 0
        self.bytes_scanned = 0
        self.bytes_processed = 0
        self.bytes_returned = 0

    def cost(self):
        """
        Estimates the cost of the scan operation based on S3 pricing in the following page:
        <https://aws.amazon.com/s3/pricing/>
        :return: The estimated cost of the table scan operation
        """
        return self.bytes_returned * BYTE_TO_GB * SQLTableScanMetrics.COST_S3_DATA_RETURNED_PER_GB + \
               self.bytes_scanned * BYTE_TO_GB * SQLTableScanMetrics.COST_S3_DATA_SCANNED_PER_GB

    def __repr__(self):
        return {
            'elapsed_time': round(self.elapsed_time(), 5),
            'rows_returned': self.rows_returned,
            'query_bytes': self.query_bytes,
            'bytes_scanned': self.bytes_scanned,
            'bytes_processed': self.bytes_processed,
            'bytes_returned': self.bytes_returned,
            'time_to_first_response': round(self.time_to_first_response, 5),
            'time_to_first_record_response':
                None if self.time_to_first_record_response is None
                else round(self.time_to_first_record_response, 5),
            'time_to_last_record_response':
                None if self.time_to_last_record_response is None
                else round(self.time_to_last_record_response, 5),
            'cost': self.cost()

        }.__repr__()


class SQLTableScan(Operator):
    """Represents a table scan operator which reads from an s3 table and emits tuples to consuming operators
    as they are received. Generally starting this operator is what begins a query.

    """

    def on_receive(self, ms, producer_name):
        for m in ms:
            if type(m) is StringMessage:
                self.s3sql = m.string_
                if self.async_:
                    self.query_plan.send(StartMessage(), self.name)
                else:
                    self.run()
            else:
                raise Exception("Unrecognized message {}".format(m))

    def __init__(self, s3key, s3sql, use_pandas, name, query_plan, log_enabled):
        """Creates a new Table Scan operator using the given s3 object key and s3 select sql

        :param s3key: The object key to select against
        :param s3sql: The s3 select sql
        """

        super(SQLTableScan, self).__init__(name, SQLTableScanMetrics(), query_plan, log_enabled)

        # Boto is not thread safe so need one of these per scan op
        cfg = Config(region_name="us-east-1", parameter_validation=False, max_pool_connections=10)
        session = Session()
        self.s3 = session.client('s3', config=cfg)

        self.s3key = s3key
        self.s3sql = s3sql

        self.use_pandas = use_pandas

    def run(self):
        """Executes the query and begins emitting tuples.

        :return: None
        """
        self.do_run()

    def do_run(self):

        self.op_metrics.timer_start()

        if self.log_enabled:
            print("{} | {}('{}') | Started"
                  .format(time.time(), self.__class__.__name__, self.name))

        if self.use_pandas:
            cur = self.execute_pandas_query(self)
        else:
            cur = self.execute_py_query(self)

        self.op_metrics.bytes_scanned = cur.bytes_scanned
        self.op_metrics.bytes_processed = cur.bytes_processed
        self.op_metrics.bytes_returned = cur.bytes_returned
        self.op_metrics.time_to_first_record_response = cur.time_to_first_record_response
        self.op_metrics.time_to_last_record_response = cur.time_to_last_record_response

        if not self.is_completed():
            self.complete()

        self.op_metrics.timer_stop()

    @staticmethod
    def execute_py_query(op):
        cur = Cursor(op.s3).select(op.s3key, op.s3sql)
        op.op_metrics.query_bytes = cur.query_bytes
        tuples = cur.execute()
        first_tuple = True
        for t in tuples:

            if op.is_completed():
                break

            op.op_metrics.rows_returned += 1

            if first_tuple:
                # Create and send the record field names
                it = IndexedTuple.build_default(t)
                first_tuple = False

                if op.log_enabled:
                    print("{}('{}') | Sending field names: {}"
                          .format(op.__class__.__name__, op.name, it.field_names()))

                op.send(TupleMessage(Tuple(it.field_names())), op.consumers)

            # if op.log_enabled:
            #     print("{}('{}') | Sending field values: {}".format(op.__class__.__name__, op.name, t))

            op.send(TupleMessage(Tuple(t)), op.consumers)
        return cur

    @staticmethod
    def execute_pandas_query(op):
        cur = PandasCursor(op.s3).select(op.s3key, op.s3sql)
        dfs = cur.execute()
        op.op_metrics.query_bytes = cur.query_bytes
        op.op_metrics.time_to_first_response = op.op_metrics.elapsed_time()
        first_tuple = True


        buffer_ = pd.DataFrame()
        for df in dfs:

            assert (len(df) > 0)

            if first_tuple:
                assert (len(df.columns.values) > 0)
                op.send(TupleMessage(Tuple(df.columns.values)), op.consumers)
                first_tuple = False

                if op.log_enabled:
                    print("{}('{}') | Sending field names: {}"
                          .format(op.__class__.__name__, op.name, df.columns.values))

            op.op_metrics.rows_returned += len(df)

            # if op.log_enabled:
            #     print("{}('{}') | Sending field values: {}".format(op.__class__.__name__, op.name, df))

            op.send(df, op.consumers)
            #buffer_ = pd.concat([buffer_, df], axis=0, sort=False, ignore_index=True, copy=False)
            #if len(buffer_) >= 8192:
            #    op.send(buffer_, op.consumers)
            #    buffer_ = pd.DataFrame()

        if len(buffer_) > 0:
            op.send(buffer_, op.consumers)
            del buffer_

        return cur
