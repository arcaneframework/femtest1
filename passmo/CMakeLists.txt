cmake_minimum_required(VERSION 3.21)

project(Passmo LANGUAGES C CXX)
find_package(Arcane REQUIRED)

add_executable(Passmo
  TypesElastodynamic.h
  Elastodynamic_axl.h
  ElastodynamicModule.h
  ElastodynamicModule.cc
  main.cc
  Integer3std.h
  utilFEM.h
  utilFEM.cc
  )

arcane_generate_axl(Elastodynamic)
arcane_add_arcane_libraries_to_target(Passmo)

target_include_directories(Passmo PUBLIC . ../fem1 ${CMAKE_CURRENT_BINARY_DIR})
configure_file(Passmo.config ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
configure_file(Passmo.arc ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
configure_file(plancher.msh ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
configure_file(plancher.quad4.msh ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)

target_link_libraries(Passmo PUBLIC FemUtils)
