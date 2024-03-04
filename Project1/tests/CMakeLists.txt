
set(tests_tcp basic_tcp tcp_long)
set(tests_udp basic_udp)

foreach(test IN LISTS tests_tcp)
    file(WRITE ${CMAKE_BINARY_DIR}/tests/tmp/${test}.sh
    "#!${BASH}\n"
    "${IPKPD} -p 9124 &\n"
    "pid=$!\n"
    "sleep 0.1\n"
    "${CMAKE_BINARY_DIR}/ipkcpc -h 127.0.0.1 -p 9124 -m TCP < ${CMAKE_SOURCE_DIR}/tests/${test}.in | diff - ${CMAKE_SOURCE_DIR}/tests/${test}.out\n"
    "ret=$?\n"
    "kill -9 $pid\n"
    "exit $ret\n"
    )

    file(COPY ${CMAKE_BINARY_DIR}/tests/tmp/${test}.sh DESTINATION ${CMAKE_BINARY_DIR}/tests
    FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)

    add_test(${test} ${CMAKE_BINARY_DIR}/tests/${test}.sh)
endforeach()

foreach(test IN LISTS tests_udp)
    file(WRITE ${CMAKE_BINARY_DIR}/tests/tmp/${test}.sh
    "#!${BASH}\n"
    "${IPKPD} -p 9024 -m udp &\n"
    "pid=$!\n"
    "sleep 0.1\n"
    "${CMAKE_BINARY_DIR}/ipkcpc -h 127.0.0.1 -p 9024 -m UDP < ${CMAKE_SOURCE_DIR}/tests/${test}.in | diff - ${CMAKE_SOURCE_DIR}/tests/${test}.out\n"
    "ret=$?\n"
    "kill -9 $pid\n"
    "exit $ret\n"
    )

    file(COPY ${CMAKE_BINARY_DIR}/tests/tmp/${test}.sh DESTINATION ${CMAKE_BINARY_DIR}/tests
    FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)

    add_test(${test} ${CMAKE_BINARY_DIR}/tests/${test}.sh)
endforeach()


file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/tests/tmp)
