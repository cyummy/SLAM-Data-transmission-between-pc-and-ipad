cmake_minimum_required(VERSION 2.5)

project( DisplayImage )
find_package( OpenCV REQUIRED )
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)

include_directories( ${OpenCV_INCLUDE_DIRS} )
include_directories(${PNG_INCLUDE_DIR})
include_directories(${JPEG_INCLUDE_DIR})

add_executable( server_both server_both.cpp )
target_link_libraries( server_both ${OpenCV_LIBS} ${PNG_LIBRARY} ${JPEG_LIBRARY})
