# pybind11 is already available from the root CMakeLists.txt

pybind11_add_module(silex
    module.cpp
    bind_constants.cpp
    bind_structs.cpp
    bind_interfaces.cpp
    bind_resolver.cpp
    bind_segmenters.cpp
    bind_functors.cpp
    bind_core.cpp
)

target_link_libraries(silex PRIVATE silex_core)

# Bindings use spdlog directly for set_log_level binding
if(SILEX_PYTHON_WHEEL_LAYOUT AND TARGET spdlog::spdlog_header_only)
    target_link_libraries(silex PRIVATE spdlog::spdlog_header_only)
elseif(TARGET spdlog::spdlog)
    target_link_libraries(silex PRIVATE spdlog::spdlog)
elseif(TARGET spdlog)
    target_link_libraries(silex PRIVATE spdlog)
endif()

# bind_core.cpp transitively includes nlohmann/json.hpp via SchemaValidator.h
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(silex PRIVATE nlohmann_json::nlohmann_json)
endif()

# bind_core.cpp transitively includes json/value.h via FileSchemaLoader.h
if(SILEX_PYTHON_WHEEL_LAYOUT AND TARGET jsoncpp_static)
    target_link_libraries(silex PRIVATE jsoncpp_static)
elseif(TARGET jsoncpp_lib)
    target_link_libraries(silex PRIVATE jsoncpp_lib)
elseif(TARGET jsoncpp_static)
    target_link_libraries(silex PRIVATE jsoncpp_static)
elseif(TARGET JsonCpp::JsonCpp)
    target_link_libraries(silex PRIVATE JsonCpp::JsonCpp)
endif()

target_include_directories(silex PRIVATE
    ${CMAKE_SOURCE_DIR}/source/silex_core/src
    ${CMAKE_SOURCE_DIR}/source/silex_core/bindings
)

if(SILEX_PYTHON_WHEEL_LAYOUT)
    if(APPLE)
        set_target_properties(silex PROPERTIES INSTALL_RPATH "@loader_path")
    elseif(UNIX)
        set_target_properties(silex PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()
endif()

# Install the Python module directly into the requested package layout
install(TARGETS silex
    LIBRARY DESTINATION ${SILEX_PYTHON_MODULE_INSTALL_DIR}
    RUNTIME DESTINATION ${SILEX_PYTHON_MODULE_INSTALL_DIR}
)
