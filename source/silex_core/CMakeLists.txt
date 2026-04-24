## silex_core - Core path resolution library

set(SILEX_CORE_SOURCES
    src/registry/Registry.cpp
    src/registry/BuiltinRegistrar.cpp
    src/schema/FileSchemaLoader.cpp
    src/schema/SchemaValidator.cpp
    src/schema/SchemaUtils.cpp
    src/expression/ExpressionParser.cpp
    src/expression/ExpressionEvaluator.cpp
    src/resolver/GenericResolver.cpp
    src/resolver/private/GenericResolver_p.cpp
    src/resolver/ResolverHelpers.cpp
    src/functors/CaseConversionFunctor.cpp
    src/functors/CaseSplitFunctor.cpp
    src/functors/GlobFunctor.cpp
    src/functors/GlobTagFunctor.cpp
    src/functors/LexiconFunctor.cpp
    src/segmenters/FilesystemSegmenter.cpp
    src/segmenters/URISegmenter.cpp
    src/util/Logging.cpp
    src/util/Utils.cpp
)

set(SILEX_PUBLIC_HEADERS
    include/silex/export.h
    include/silex/constants.h
    include/silex/structs.h
    include/silex/GenericResolver.h
    include/silex/interfaces/IFunctor.h
    include/silex/interfaces/ISegmenter.h
    include/silex/interfaces/ILoader.h
    include/silex/interfaces/IResolver.h
    include/silex/interfaces/IExpressionParser.h
    include/silex/interfaces/IExpressionEvaluator.h
)

set(SILEX_PRIVATE_HEADERS
    src/registry/Registry.h
    src/registry/BuiltinRegistrar.h
    src/schema/FileSchemaLoader.h
    src/schema/SchemaValidator.h
    src/schema/SchemaUtils.h
    src/expression/ExpressionParser.h
    src/expression/ExpressionEvaluator.h
    src/expression/DAG.h
    src/resolver/ResolverHelpers.h
    src/functors/CaseConversionFunctor.h
    src/functors/CaseSplitFunctor.h
    src/functors/GlobFunctor.h
    src/functors/GlobTagFunctor.h
    src/functors/LexiconFunctor.h
    src/segmenters/FilesystemSegmenter.h
    src/segmenters/URISegmenter.h
    src/util/Logging.h
    src/util/Utils.h
)

add_library(silex_core SHARED
    ${SILEX_CORE_SOURCES}
    ${SILEX_PUBLIC_HEADERS}
    ${SILEX_PRIVATE_HEADERS}
)

target_compile_definitions(silex_core PRIVATE SILEX_CORE_EXPORTS)

# TODO: Remove once tests use an OBJECT library instead of linking against the DLL.
# SILEX_API marks the intended public API; this exports everything for test access.
set_target_properties(silex_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(silex_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# json5cpp is header-only, fetched at configure time
if(json5cpp_SOURCE_DIR)
    target_include_directories(silex_core PRIVATE ${json5cpp_SOURCE_DIR})
endif()

# Link dependencies (PRIVATE - not exposed to consumers)
if(SILEX_PYTHON_WHEEL_LAYOUT AND TARGET jsoncpp_static)
    target_link_libraries(silex_core PRIVATE jsoncpp_static)
elseif(TARGET jsoncpp_lib)
    target_link_libraries(silex_core PRIVATE jsoncpp_lib)
elseif(TARGET jsoncpp_static)
    target_link_libraries(silex_core PRIVATE jsoncpp_static)
elseif(TARGET JsonCpp::JsonCpp)
    target_link_libraries(silex_core PRIVATE JsonCpp::JsonCpp)
endif()

if(SILEX_PYTHON_WHEEL_LAYOUT AND TARGET spdlog::spdlog_header_only)
    target_link_libraries(silex_core PRIVATE spdlog::spdlog_header_only)
elseif(TARGET spdlog::spdlog)
    target_link_libraries(silex_core PRIVATE spdlog::spdlog)
elseif(TARGET spdlog)
    target_link_libraries(silex_core PRIVATE spdlog)
endif()

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(silex_core PRIVATE nlohmann_json::nlohmann_json)
endif()

target_compile_features(silex_core PUBLIC cxx_std_17)

if(SILEX_PYTHON_WHEEL_LAYOUT)
    if(APPLE)
        set_target_properties(silex_core PROPERTIES INSTALL_RPATH "@loader_path")
    elseif(UNIX)
        set_target_properties(silex_core PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()
endif()

# Install library
install(TARGETS silex_core
    EXPORT SilexTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${SILEX_PYTHON_RUNTIME_INSTALL_DIR}
    RUNTIME DESTINATION ${SILEX_PYTHON_RUNTIME_INSTALL_DIR}
)

# Install public headers preserving directory structure
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/silex
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

message(STATUS "Configured silex_core")
