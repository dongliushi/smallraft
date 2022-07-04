file(GLOB SRC *.cc)
file(GLOB HEADER *.h)

add_library(smallraft SHARED ${SRC})

# target_link_libraries(smalljrpc PUBLIC smalljson smallnet)

# install(TARGETS smalljrpc 
#    LIBRARY DESTINATION lib
#    )

# install(FILES ${HEADER} DESTINATION include/smalljrpc)