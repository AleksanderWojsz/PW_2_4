cmake_minimum_required(VERSION 3.25)
project(PW_2_4 C)

set(CMAKE_C_STANDARD 11)

add_executable(PW_2_4 main.c
        mimpi.c
        mimpirun.c
        examples/lot_of_messages.c
        examples/extended_pipe_closed.c
        examples/big_message.c)
