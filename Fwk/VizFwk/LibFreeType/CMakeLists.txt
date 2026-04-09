project(LibFreeType)


# Use our strict compile flags
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  ${CEE_STRICT_CXX_FLAGS}")


find_package(Freetype REQUIRED)

include_directories(../LibCore)
include_directories(../LibIo)
include_directories(../LibGeometry)
include_directories(../LibRender)


set(CEE_HEADER_FILES
cvfFreeTypeFont.h
cvfSdfFont.h
)

set(CEE_SOURCE_FILES
cvfFreeTypeFont.cpp
cvfSdfFont.cpp
)

add_library(${PROJECT_NAME} ${CEE_HEADER_FILES} ${CEE_SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} Freetype::Freetype)
target_include_directories(${PROJECT_NAME} PUBLIC ${FREETYPE_INCLUDE_DIRS})
