if(DFK_HAVE_UCONTEXT_H)
  add_definitions(-DHAVE_UCONTEXT_H)
endif()
if(DFK_HAVE_SETJMP_H)
  add_definitions(-DHAVE_SETJMP_H)
endif()
if(DFK_HAVE_SIGALTSTACK)
  add_definitions(-DHAVE_SIGALTSTACK)
endif()
add_library(coro STATIC
  libcoro/coro.c)

if(BUILD_SHARED_LIBS)
  set_target_properties(coro
    PROPERTIES
    COMPILE_FLAGS -fPIC)
endif()

install(FILES libcoro/coro.h DESTINATION include/dfk/thirdparty/libcoro/)
