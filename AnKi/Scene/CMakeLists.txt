file(GLOB_RECURSE sources *.cpp)
file(GLOB_RECURSE headers *.h)
add_library(AnKiScene ${sources} ${headers})
target_compile_definitions(AnKiScene PRIVATE -DANKI_SOURCE_FILE)
target_link_libraries(AnKiScene AnKiResource AnKiScript AnKiCollision AnKiPhysics AnKiGpuMemory)
