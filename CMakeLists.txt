cmake_minimum_required(VERSION 3.13)
project(map-viewer-thing VERSION 0.1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (NOT CMAKE_BUILD_TYPE)
	message(WARNING "CMAKE_BUILD_TYPE not set, defaulting to Release")
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wnon-virtual-dtor -Wl,--export-dynamic")
if (EMSCRIPTEN)
	# emcc doesn't recognize -Og
	set(CMAKE_CXX_FLAGS_DEBUG   "-DGAME_BUILD_DEBUG=1 -O2")
	set(CMAKE_CXX_FLAGS_RELEASE "-DGAME_BUILD_RELEASE=1 -O2")
else()
	set(CMAKE_CXX_FLAGS_DEBUG   "-DGAME_BUILD_DEBUG=1 -Og -g")
	set(CMAKE_CXX_FLAGS_RELEASE "-DGAME_BUILD_RELEASE=1 -O2")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

option(PORTABLE_BUILD      "Portable build, include all dependancies in the install" OFF)
option(PRECOMPILED_HEADERS "Build with precompiled headers (experimental)"           OFF)
include(GNUInstallDirs)

if (EXISTS ${PROJECT_SOURCE_DIR}/grend)
	message(STATUS "Found grend subdirectory, using that as library")
	set(GREND_PATH ./grend)
endif()

if (EXISTS ${PROJECT_SOURCE_DIR}/bullet)
	message(STATUS "Found bullet subdirectory, using that as library")
	set(BULLET_PHYSICS_SOURCE_DIR "${PROJECT_SOURCE_DIR}/bullet")
endif()

if (GREND_PATH)
	message(STATUS "GREND_PATH: ${GREND_PATH}")
	add_subdirectory(${GREND_PATH})
endif()

if (BULLET_PHYSICS_SOURCE_DIR)
	message(STATUS "BULLET_PHYSICS_SOURCE_DIR: ${BULLET_PHYSICS_SOURCE_DIR}")
	add_subdirectory(${BULLET_PHYSICS_SOURCE_DIR})
endif()

set (DEMO_LINK_LIBS)
set (DEMO_LINK_OPTIONS)
set (DEMO_PREFIX)

if (ANDROID OR GREND_PATH)
	# everything needed should be pulled in here
	find_library(Grend Grend)
	list(APPEND DEMO_LINK_LIBS Grend)

elseif(EMSCRIPTEN)
	#link_directories("${CMAKE_INSTALL_PREFIX}/lib")
	#find_library(Grend Grend)
	#list(APPEND DEMO_LINK_LIBS Grend)

	set(CMAKE_PREFIX_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
	set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_PREFIX_PATH}")
	message("PREFIX PATH: $ENV{PKG_CONFIG_PATH}")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(Grend REQUIRED IMPORTED_TARGET grend)
	list(APPEND DEMO_LINK_LIBS PkgConfig::Grend)

# TODO: assume unixen, presumably there'll be different library lookup
#       stuff for windows/etc
else()
	#add_compile_options(-DDEMO_PREFIX="${CMAKE_INSTALL_PREFIX}/share/landscape-demo/")

	find_package(PkgConfig REQUIRED)
	pkg_check_modules(Grend REQUIRED IMPORTED_TARGET grend)
	list(APPEND DEMO_LINK_LIBS PkgConfig::Grend)
endif()

set(LANDSCAPE_DEMO_SRC
	src/components/health.cpp
	src/components/healthbar.cpp
	src/components/area.cpp
	src/components/inputHandler.cpp
	src/components/boxSpawner.cpp
	src/components/team.cpp
	src/components/timedLifetime.cpp
	src/components/itemPickup.cpp
	src/components/inventory.cpp
	src/components/playerInfo.cpp
	src/components/explosionParticles.cpp
	src/components/actions/destructors.cpp

	src/entities/enemy.cpp
	src/entities/enemySpawner.cpp
	src/entities/enemyCollision.cpp
	src/entities/flag.cpp
	src/entities/player.cpp
	src/entities/targetArea.cpp
	src/entities/projectile.cpp
	src/entities/killedParticles.cpp
	src/entities/amulet.cpp
	src/entities/items/destructors.cpp

	src/logic/UI.cpp
	src/logic/levelController.cpp
	src/logic/main.cpp
	src/logic/wfcGenerator.cpp
	src/logic/generatorEvent.cpp
	src/logic/globalMessages.cpp
	src/logic/lootSystem.cpp

	src/logic/projalphaView.cpp
	src/logic/view/floorStates.cpp
	src/logic/view/main.cpp
	src/logic/view/UI.cpp

	src/logic/tests/default.cpp

	src/nuklear/impl/nuklear.c
)

message(STATUS "CMake host: ${CMAKE_HOST_SYSTEM_NAME}")
if (ANDROID)
	message(STATUS "Setting library for android")
	set(TARGET_NAME main)
	add_library(main SHARED ${LANDSCAPE_DEMO_SRC})
	install(TARGETS main DESTINATION ${CMAKE_INSTALL_LIBDIR})
	list(APPEND DEMO_LINK_LIBS SDL2 SDL2main)

else()
	message(STATUS "Setting standalone executable")
	set(TARGET_NAME landscape-demo)
	add_executable(landscape-demo ${LANDSCAPE_DEMO_SRC})
	install(TARGETS landscape-demo DESTINATION ${CMAKE_INSTALL_BINDIR})

	if(MINGW OR WIN32 OR CYGWIN)
		message(STATUS "Building for/on windows")
		#add_compile_options(-lopengl32)
		# XXX: should be part of grend target compile options...
		add_compile_options(-D_WIN32)
		find_library(opengl32 opengl32)
		list(APPEND DEMO_LINK_LIBS opengl32)

	elseif (EMSCRIPTEN)
		message(STATUS "Building for emscripten")
		set(EMSCRIPTEN_MORE_STUFF
			--preload-file=/tmp/webbuild/share/grend@/
			--preload-file=${PROJECT_SOURCE_DIR}/assets/@/assets
			--shell-file=${PROJECT_SOURCE_DIR}/shell.html
		)

		set(EMSCRIPTEN_USE_STUFF
			-sUSE_SDL=2
			-sMIN_WEBGL_VERSION=2
			-sMAX_WEBGL_VERSION=2
			-sUSE_WEBGL2=1
			-sERROR_ON_UNDEFINED_SYMBOLS=0
			-sTOTAL_MEMORY=1048576000
			-sOFFSCREEN_FRAMEBUFFER=1
			-sFULL_ES3=1
		)

		set(CMAKE_EXECUTABLE_SUFFIX ".html")

		target_link_directories(landscape-demo PUBLIC "${CMAKE_INSTALL_PREFIX}/lib")

		#find_library(Grend Grend)
		#find_library(bullet bullet)
		#list(APPEND DEMO_LINK_LIBS Grend bullet)

		#target_link_libraries(landscape-demo PUBLIC ${DEMO_LINK_LIBS})
		target_compile_options(landscape-demo PUBLIC ${EMSCRIPTEN_USE_STUFF} ${EMSCRIPTEN_MORE_STUFF})
		target_link_options(landscape-demo  PUBLIC ${EMSCRIPTEN_USE_STUFF} ${EMSCRIPTEN_MORE_STUFF})

	else()
		# pthreads on unixen
		message(STATUS "Building for/on unixen, adding -pthread")
		list(APPEND DEMO_LINK_OPTIONS -pthread)
	endif()
endif()

### TODO: configurable data path
install(DIRECTORY assets DESTINATION ${CMAKE_INSTALL_DATADIR}/proj)

target_include_directories(${TARGET_NAME} PUBLIC "${PROJECT_BINARY_DIR}")
target_include_directories(${TARGET_NAME} PUBLIC src)
target_include_directories(${TARGET_NAME} PUBLIC wfc/include)
target_include_directories(${TARGET_NAME} PUBLIC Grend)
target_link_libraries(${TARGET_NAME} ${DEMO_LINK_LIBS})
target_link_options(${TARGET_NAME} PUBLIC ${DEMO_LINK_OPTIONS})

if (PRECOMPILED_HEADERS)
target_precompile_headers(${TARGET_NAME} PRIVATE
	# this is the way described in the cmake documentation to limit
	# a header to one language... jeeeeeez this is terrible man
	"$<$<COMPILE_LANGUAGE:CXX>:<components/area.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/boxSpawner.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/health.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/healthbar.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/inputHandler.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/team.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/timedLifetime.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/itemPickup.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/inventory.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/playerInfo.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/actions/action.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/actions/Throwable.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/actions/Wieldable.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<components/removeBodiesOnWorldCollision.hpp$<ANGLE-R>>"

	"$<$<COMPILE_LANGUAGE:CXX>:<entities/enemy.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/enemyCollision.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/enemySpawner.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/flag.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/killedParticles.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/player.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/projectile.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/targetArea.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/amulet.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/items/items.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/items/coinPickup.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/items/flareItem.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/items/chestItem.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<entities/items/healthPickup.hpp$<ANGLE-R>>"

	"$<$<COMPILE_LANGUAGE:CXX>:<logic/UI.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<logic/levelController.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<logic/generatorEvent.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<logic/bspGenerator.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<logic/array2D.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<logic/projalphaView.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<logic/wfcGenerator.hpp$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<logic/tests/tests.hpp$<ANGLE-R>>"

	"$<$<COMPILE_LANGUAGE:CXX>:<utility/serializer.hpp$<ANGLE-R>>"

	"$<$<COMPILE_LANGUAGE:CXX>:<nuklear/impl/nuklear.h$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<nuklear/nuklear.h$<ANGLE-R>>"
	"$<$<COMPILE_LANGUAGE:CXX>:<nuklear/canvas.h$<ANGLE-R>>"
)
endif()

if (PORTABLE_BUILD)
	target_compile_options(${TARGET_NAME} PUBLIC -DDEMO_PREFIX="./share/proj/")
else()
	target_compile_options(${TARGET_NAME} PUBLIC
	                       -DDEMO_PREFIX="${CMAKE_INSTALL_PREFIX}/share/proj/")
endif()


if (PORTABLE_BUILD)
	install(FILES run/run-linux64.sh
	        DESTINATION ${CMAKE_INSTALL_PREFIX}
	        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
	                    GROUP_EXECUTE GROUP_READ)
	# TODO: should also output a launcher script
	install(CODE "set(CMAKE_INSTALL_LIBDIR \"${CMAKE_INSTALL_LIBDIR}\")")
	install(CODE
	[[
		#include(BundleUtilities)
		#	fixup_bundle("landscape-test" "" "asdf")


		#file(GET_RUNTIME_DEPENDENCIES
		#	RESOLVED_DEPENDENCIES_VAR   runtime_libs
		#	UNRESOLVED_DEPENDENCIES_VAR unresolved_libs
		#	EXECUTABLES landscape-demo
		#	# have to bring your own libGL
		#	PRE_EXCLUDE_REGEXES ".*/lib32/.*"
		#	POST_EXCLUDE_REGEXES ".*libGL.so" ".*opengl32.dll"
		#)

		# https://discourse.cmake.org/t/file-get-runtime-dependencies-issues/2574
		# ugh
		execute_process(COMMAND ldd landscape-demo OUTPUT_VARIABLE ldd_out)
		string (REPLACE "\n" ";" lines "${ldd_out}")
		foreach (line ${lines})
			string (REGEX REPLACE "^.* => | \(.*\)" "" pruned "${line}")
			string (STRIP "${pruned}" dep)
			message("LIB: ${pruned} => ${dep}")
			if (${dep} MATCHES ".*libGL.so|.*opengl32.dll")
				continue()
			endif()

			if (IS_ABSOLUTE ${dep})
				message("ADDED: ${pruned} => ${dep}")
				list (APPEND runtime_libs ${dep})
			endif()
		endforeach()

		message("RESOLVED:   ${runtime_libs}")
		message("UNRESOLVED: ${unresolved_libs}")

		foreach(_lib ${runtime_libs})
			file(INSTALL
				DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
				TYPE SHARED_LIBRARY
				FOLLOW_SYMLINK_CHAIN
				FILES "${_lib}")
		endforeach()
	]])
endif()

include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_PACKAGE_VERSION_MAJOR "${Grend_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${Grend_VERSION_MINOR}")
include(CPack)
