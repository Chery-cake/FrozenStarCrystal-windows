# ==============================================================================
# Threading Dependencies
# ==============================================================================

include(FetchContent)

find_package(TBB REQUIRED)

# Create an interface library to abstract the actual TBB target name
add_library(tbb_engine INTERFACE)

if(TARGET TBB::tbb)
    target_link_libraries(tbb_engine INTERFACE TBB::tbb)
elseif(TARGET tbb)
    target_link_libraries(tbb_engine INTERFACE tbb)
else()
    # Fallback for older FindTBB.cmake that sets TBB_LIBRARIES
    target_link_libraries(tbb_engine INTERFACE ${TBB_LIBRARIES})
    target_include_directories(tbb_engine INTERFACE ${TBB_INCLUDE_DIRS})
endif()

message(STATUS "Threading dependencies configured")
