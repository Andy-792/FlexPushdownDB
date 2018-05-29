# -*- coding: utf-8 -*-
"""Metrio support

"""
from util.timer import Timer


class OpMetrics(object):

    def __init__(self):
        self.__timer = Timer()

    def timer_start(self):
        self.__timer.start()

    def timer_stop(self):
        self.__timer.stop()

    def elapsed_time(self):
        return self.__timer.elapsed()

    @staticmethod
    def print_metrics(op_list):
        print("")
        print("Metrics")
        print("-------")
        print("")
        for op in op_list:
            OpMetrics.print_metric(op)

    @staticmethod
    def print_metric(op):
        print("{}: {}".format(op, op.op_metrics))

    def __repr__(self):
        return {'elapsed_time': round(self.elapsed_time(), 5)}.__repr__()