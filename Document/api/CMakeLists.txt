add_library(DocumentAPI SHARED)
add_library(Document::API ALIAS DocumentAPI)

target_sources(DocumentAPI
    PRIVATE
        src/DocumentAction.cpp
        src/DocumentLink.cpp
        src/DocumentFacade.cpp
)

target_include_directories(DocumentAPI
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Document/API
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

# TODO: Get rid of Qt6::Core primitive types to be independent from it on core
target_link_libraries(DocumentAPI
    PUBLIC
        Qt6::Core
        Qt6::Gui
)
