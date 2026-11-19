# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""One CPU matrix write at the application's unsealed ordinal."""
import ctypes as c
import ovstage


class TransformWriter:
    def __init__(self, stage, path):
        self.stage = stage
        self.paths = ovstage.PathDictionary(stage)
        self.path_list = None
        self.query = None
        try:
            self.path_list = self.paths.create_path_list_from_strings([path])
            self.query = stage.query_from_path_list(self.path_list)
        except BaseException:
            self.close()
            raise

    def write(self, matrix, ordinal):
        data = (c.c_double * 16)(*matrix)
        shape = (c.c_int64 * 1)(1)
        tensor = ovstage.DLTensor(data=c.cast(data, c.c_void_p), device=(ovstage.DLDeviceType.kDLCPU, 0),
                                 ndim=1, dtype=(ovstage.DLDataTypeCode.kDLFloat, 64, 16), shape=shape)
        self.stage.write_attribute(self.query, "omni:xform", ordinal=ordinal, tensors=tensor, is_array=False,
                                   semantic=ovstage.AttributeSemantic.MATRIX).wait()

    def close(self):
        if self.query is not None:
            self.query.release().wait()
            self.query = None
        if self.path_list is not None:
            self.paths.destroy_path_list(self.path_list)
            self.path_list = None
        self.paths.destroy()
