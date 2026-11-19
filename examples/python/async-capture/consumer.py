# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""A single pending owned frame: replace stale work instead of blocking XR."""
from dataclasses import dataclass
import threading


@dataclass(frozen=True)
class OwnedFrame:
    sequence: int
    width: int
    height: int
    pitch: int
    rgba: bytes


class Consumer:
    def __init__(self, delay_ms):
        self.delay = delay_ms / 1000
        self.condition = threading.Condition()
        self.stopping = threading.Event()
        self.pending = None
        self.error = None
        self.offered = self.dropped = self.consumed = 0
        self.last_sequence = self.checksum = 0
        self.thread = threading.Thread(target=self._run, name="owned-frame-consumer")
        self.thread.start()

    def offer(self, frame):
        with self.condition:
            if self.error:
                raise RuntimeError("Capture consumer failed") from self.error
            if self.stopping.is_set():
                raise RuntimeError("Capture consumer is closed")
            if self.pending is not None:
                self.dropped += 1
            self.pending = frame
            self.offered += 1
            self.condition.notify()

    def consume(self, frame):
        # Substitute an encoder/analysis operation here. It owns these bytes;
        # it must never call OVXR or retain a borrowed GPU pointer.
        self.stopping.wait(self.delay)
        self.checksum = sum(frame.rgba) & 0xffffffff
        self.last_sequence = frame.sequence
        self.consumed += 1

    def _run(self):
        try:
            while True:
                with self.condition:
                    self.condition.wait_for(lambda: self.pending is not None or self.stopping.is_set())
                    if self.pending is None:
                        return
                    frame, self.pending = self.pending, None
                self.consume(frame)
        except BaseException as error:
            with self.condition:
                self.error = error

    def close(self):
        self.stopping.set()
        with self.condition:
            self.condition.notify()
        self.thread.join()  # Finish active work and the one remaining pending frame.
        if self.error:
            raise RuntimeError("Capture consumer failed") from self.error
