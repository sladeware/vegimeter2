#!/usr/bin/env python
#
# Copyright (c) 2013 Sladeware LLC.

import bb.app
from bb.app.hardware.devices.boards import P8X32A_QuickStartBoard

board = P8X32A_QuickStartBoard()
vegimeter2 = bb.app.Mapping(
  name='vegimeter2',
  #thread_distributor=bb.app.DummyThreadDistributor(),
  processor=board.get_processor(),
  threads=[
    # Add user interface thread
    bb.app.Thread('ENGINE', 'engine_runner', port=bb.app.os.Port(10)),
  ]
)
