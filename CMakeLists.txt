cmake_minimum_required(VERSION 3.12)
project(Cinecoder.Samples)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_scripts")
find_package(Cinecoder REQUIRED)
find_package(Cinecoder.Plugin.Multiplexers REQUIRED)

if(UNIX AND NOT APPLE)
find_package(CUDAConvertLib REQUIRED)
endif()

if(APPLE)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/_bin/macosx")
else()
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/_bin/linux")
endif()

#add_custom_command(
#	TARGET Cinecoder.Samples
#	POST_BUILD 
#	COMMAND ${CMAKE_COMMAND} -E copy
#	"${Cinecoder_LIBRARY_DIRS}/*"
#	"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
#)

file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})

if(APPLE)
configure_file(${Cinecoder_LIBRARY_DIRS}/libCinecoder.dylib ${EXECUTABLE_OUTPUT_PATH} COPYONLY)
configure_file(${Cinecoder.Plugin.Multiplexers_LIBRARY_DIRS}/libMXFToolkit_portable.dylib  ${EXECUTABLE_OUTPUT_PATH} COPYONLY)
configure_file(${Cinecoder.Plugin.Multiplexers_LIBRARY_DIRS}/libCinecoder.Plugin.Multiplexers.dylib ${EXECUTABLE_OUTPUT_PATH} COPYONLY)
else()
configure_file(${Cinecoder_LIBRARY_DIRS}/libCinecoder.so ${EXECUTABLE_OUTPUT_PATH} COPYONLY)
configure_file(${Cinecoder_LIBRARY_DIRS}/libd2cudalib.so ${EXECUTABLE_OUTPUT_PATH} COPYONLY)
configure_file(${Cinecoder.Plugin.Multiplexers_LIBRARY_DIRS}/libMXFToolkit_portable.so  ${EXECUTABLE_OUTPUT_PATH} COPYONLY)
configure_file(${Cinecoder.Plugin.Multiplexers_LIBRARY_DIRS}/libCinecoder.Plugin.Multiplexers.so ${EXECUTABLE_OUTPUT_PATH} COPYONLY)
configure_file(${CUDAConvertLib_LIBRARY_DIRS}/libcudaconvertlib.so ${EXECUTABLE_OUTPUT_PATH} COPYONLY)
endif()

add_subdirectory(SimpleVideoDecoder)
add_subdirectory(SimpleVideoEncoder)
add_subdirectory(Daniel2.SimplePlayerGL)
add_subdirectory(Daniel2.DPXEncoder)
add_subdirectory(Daniel2.Benchmark)
