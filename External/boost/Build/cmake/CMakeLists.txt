PROJECT(boost)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.6)

IF(COMMAND cmake_policy)
	CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND cmake_policy)

SET(BOOST_MAJOR_VERSION 1)
SET(BOOST_MINOR_VERSION 66)
SET(BOOST_SUBMINOR_VERSION 0)
SET(BOOST_VERSION ${BOOST_MAJOR_VERSION}.${BOOST_MINOR_VERSION}.${BOOST_SUBMINOR_VERSION})

SET(BOOST_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
SET(KLAYGE_ROOT_DIR "${BOOST_PROJECT_DIR}/../..")

INCLUDE(${KLAYGE_ROOT_DIR}/cmake/Common.cmake)
INCLUDE(${KLAYGE_ROOT_DIR}/cmake/Platform.cmake)
INCLUDE(${KLAYGE_ROOT_DIR}/cmake/Compiler.cmake)
INCLUDE(${KLAYGE_ROOT_DIR}/External/Build/CMake/ExternalCommon.cmake)

SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

SET(BOOST_OUTPUT_DIR "${BOOST_PROJECT_DIR}/lib/${KLAYGE_PLATFORM_NAME}")
SET(KLAYGE_BIN_DIR "${KLAYGE_ROOT_DIR}/KlayGE/bin/${KLAYGE_PLATFORM_NAME}")
SET(REL_PATH "External/Downloads/boost_${BOOST_MAJOR_VERSION}_${BOOST_MINOR_VERSION}_${BOOST_SUBMINOR_VERSION}.patched.7z")
SET(SEVENZIP_PATH "${KLAYGE_ROOT_DIR}/External/Downloads/${KLAYGE_HOST_PLATFORM_NAME}/7z")
SET(PACKAGE_COMMIT_ID "16969865ade6366ac9a5775ae0da3402487bbc2a")
SET(PACKAGE_FILE_ID "c93ef576cb05ead2cbe6fe0386713d59bac10251")

DOWNLOAD_PACKAGE("boost" ${REL_PATH} ${PACKAGE_COMMIT_ID} ${PACKAGE_FILE_ID} "boost/version.hpp")

INCLUDE(${KLAYGE_ROOT_DIR}/cmake/ConfigBoost.cmake)

FOREACH(flag_var
		CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
	IF(KLAYGE_COMPILER_MSVC)
		STRING(REPLACE "/W4" "/W3" ${flag_var} "${${flag_var}}")
		STRING(REPLACE "/WX" "/WX-" ${flag_var} "${${flag_var}}")
	ELSE()
		STRING(REPLACE "-Werror" "-Wno-error" ${flag_var} "${${flag_var}}")
	ENDIF()
ENDFOREACH()

IF(KLAYGE_COMPILER_MSVC OR KLAYGE_COMPILER_GCC OR KLAYGE_COMPILER_CLANGC2)
	SET(WITH_FILESYSTEM OFF)
ELSE()
	SET(WITH_FILESYSTEM ON)
ENDIF()
IF(KLAYGE_IS_DEV_PLATFORM)
	SET(WITH_PROGRAM_OPTIONS ON)
ELSE()
	SET(WITH_PROGRAM_OPTIONS OFF)
ENDIF()
SET(WITH_SYSTEM ${WITH_FILESYSTEM})

OPTION(WITH_FILESYSTEM "Build with filesystem" ${WITH_FILESYSTEM})
OPTION(WITH_PROGRAM_OPTIONS "Build with program_options" ${WITH_PROGRAM_OPTIONS})
OPTION(WITH_SYSTEM "Build with system" ${WITH_SYSTEM})

IF(KLAYGE_PLATFORM_WINDOWS_STORE)
	ADD_DEFINITIONS(-DBOOST_NO_ANSI_APIS)
ENDIF()
IF(KLAYGE_COMPILER_MSVC)
	ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
	ADD_DEFINITIONS(-D_SCL_SECURE_NO_DEPRECATE)
ENDIF()
IF(KLAYGE_PLATFORM_WINDOWS)
	IF((KLAYGE_COMPILER_GCC AND (KLAYGE_ARCH_NAME STREQUAL "x64")) OR KLAYGE_COMPILER_CLANG OR KLAYGE_COMPILER_CLANGC2)
		ADD_DEFINITIONS(-DBOOST_USE_WINDOWS_H)
	ENDIF()
ENDIF()

IF(KLAYGE_PREFERRED_LIB_TYPE STREQUAL "SHARED")
	ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK=1)
	IF(KLAYGE_COMPILER_MSVC)
		SET(ABI_SUFFIX_DEBUG "-gd")
	ELSE()
		SET(ABI_SUFFIX_DEBUG "-d")
	ENDIF()
	SET(ABI_SUFFIX_OPTIMIZED "")
ELSE()
	ADD_DEFINITIONS(-DBOOST_All_STATIC_LINK=1)
	IF(KLAYGE_COMPILER_MSVC)
		SET(ABI_SUFFIX_DEBUG "-sgd")
	ELSE()
		SET(ABI_SUFFIX_DEBUG "-sd")
	ENDIF()
	SET(ABI_SUFFIX_OPTIMIZED "-s")
ENDIF()

SET(BOOST_LIB_NAME_SUFFIX_DEBUG -${KLAYGE_COMPILER_NAME}${KLAYGE_COMPILER_VERSION}-mt${ABI_SUFFIX_DEBUG}-${Boost_LIB_VERSION})
SET(BOOST_LIB_NAME_SUFFIX_OPTIMIZED -${KLAYGE_COMPILER_NAME}${KLAYGE_COMPILER_VERSION}-mt${ABI_SUFFIX_OPTIMIZED}-${Boost_LIB_VERSION})

MACRO(SETUP_BOOST_LIBRARY LIB_NAME)
	SOURCE_GROUP("Source Files" FILES ${SOURCE_FILES})

	INCLUDE_DIRECTORIES(${BOOST_PROJECT_DIR}/)
	LINK_DIRECTORIES(${BOOST_PROJECT_DIR}/lib/${KLAYGE_PLATFORM_NAME})

	ADD_LIBRARY(${LIB_NAME} ${KLAYGE_PREFERRED_LIB_TYPE}
		${SOURCE_FILES}
	)

	SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES
		ARCHIVE_OUTPUT_DIRECTORY ${BOOST_OUTPUT_DIR}
		ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${BOOST_OUTPUT_DIR}
		ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${BOOST_OUTPUT_DIR}
		ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BOOST_OUTPUT_DIR}
		ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${BOOST_OUTPUT_DIR}
		PROJECT_LABEL ${LIB_NAME}
		DEBUG_POSTFIX ""
		DEBUG_OUTPUT_NAME boost_${LIB_NAME}${BOOST_LIB_NAME_SUFFIX_DEBUG}
		RELEASE_OUTPUT_NAME boost_${LIB_NAME}${BOOST_LIB_NAME_SUFFIX_OPTIMIZED}
		RELWITHDEBINFO_OUTPUT_NAME boost_${LIB_NAME}${BOOST_LIB_NAME_SUFFIX_OPTIMIZED}
		MINSIZEREL_OUTPUT_NAME boost_${LIB_NAME}${BOOST_LIB_NAME_SUFFIX_OPTIMIZED}
	)
	IF(KLAYGE_PLATFORM_DARWIN OR KLAYGE_PLATFORM_LINUX)
		SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES
			LIBRARY_OUTPUT_DIRECTORY ${KLAYGE_BIN_DIR}
			LIBRARY_OUTPUT_DIRECTORY_DEBUG ${KLAYGE_BIN_DIR}
			LIBRARY_OUTPUT_DIRECTORY_RELEASE ${KLAYGE_BIN_DIR}
			LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${KLAYGE_BIN_DIR}
			LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${KLAYGE_BIN_DIR})
	ENDIF()

	TARGET_LINK_LIBRARIES(${LIB_NAME}
		${EXTRA_LINKED_LIBRARIES})

	IF(KLAYGE_PLATFORM_WINDOWS)
		INSTALL(TARGETS ${LIB_NAME}
			RUNTIME DESTINATION ${KLAYGE_BIN_DIR}
			LIBRARY DESTINATION ${KLAYGE_BIN_DIR}
		)
	ENDIF()
ENDMACRO()

IF(WITH_FILESYSTEM)
	ADD_SUBDIRECTORY(filesystem)
ENDIF()
IF(WITH_PROGRAM_OPTIONS)
	ADD_SUBDIRECTORY(program_options)
ENDIF()
IF(WITH_SYSTEM)
	ADD_SUBDIRECTORY(system)
ENDIF()
