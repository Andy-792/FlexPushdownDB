# -*- coding: utf-8 -*-
"""

"""
from op.operator_base import Operator


class Collate(Operator):
    """This operator simply collects emitted tuples into a list. Useful for interrogating results at the end of a
    query to see if they are what is expected.

    """

    def __init__(self):
        Operator.__init__(self)
        self.tuples = []

    def on_emit(self, t, producer=None):
        # print("Collate | {}".format(t))
        self.tuples.append(t)

    def on_done(self):
        pass