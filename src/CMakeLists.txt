add_executable(QuadHutFinder layers.c generator.c finders.c find_quadhuts.c)
target_link_libraries(QuadHutFinder Threads::Threads)
target_link_libraries(QuadHutFinder m)