include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(dfkcpp_sources
  src/buffer.cpp
  src/string_map.cpp
  src/exception.cpp
  src/context.cpp
  src/fiber.cpp
  src/http/status.cpp
  src/http/request.cpp
  src/http/request_handler.cpp
  src/http/response.cpp
  src/http/server.cpp)

if(DFK_FILESERVER)
  list(APPEND dfkcpp_sources src/middleware/fileserver.cpp)
endif()

add_library(dfkcpp ${dfkcpp_sources})

target_link_libraries(dfkcpp dfk)

set_target_properties(dfkcpp
  PROPERTIES
  VERSION ${DFK_VERSION_MAJOR}.${DFK_VERSION_MINOR}.${DFK_VERSION_PATCH}
  SOVERSION ${DFK_VERSION_MAJOR}.${DFK_VERSION_MINOR})

install(TARGETS dfkcpp DESTINATION lib)

install(DIRECTORY include/dfk DESTINATION include)
install(FILES
  include/dfk.hpp
  DESTINATION include)

configure_file(dfkcpp.pc.in "${CMAKE_CURRENT_BINARY_DIR}/dfkcpp.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dfkcpp.pc" DESTINATION lib/pkgconfig)
