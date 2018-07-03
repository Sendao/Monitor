cmake_minimum_required (VERSION 2.8)
project(Monitrage)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(sendao REQUIRED)

include_directories( /usr/local/include/sendao )
add_executable(monitrage src/config.cpp src/control.cpp src/curler_stub.cpp src/main.cpp src/monitor.cpp src/pipe.cpp src/revision.cpp src/util.cpp src/procstat.cpp src/logfiles.cpp)
target_link_libraries(monitrage ${SENDAO_LIBRARY_PATH} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS monitrage
	RUNTIME DESTINATION /var/rad)

install(FILES monitor.service DESTINATION /lib/systemd/system)
install(FILES monitor.cfg DESTINATION /etc)
install(CODE "execute_process(OUTPUT_VARIABLE _log1 COMMAND bash -c \"systemctl daemon-reload\")")
install(CODE "execute_process(OUTPUT_VARIABLE _log2 COMMAND bash -c \"systemctl enable monitor\")")
install(CODE "MESSAGE(\"\${log1} \${log2} .. done!\")")


