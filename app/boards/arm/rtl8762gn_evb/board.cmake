
board_runner_args(jlink "--device=RTL87X2G" "--speed=4000")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
