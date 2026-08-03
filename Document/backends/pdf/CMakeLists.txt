add_library(DocumentBackendPdf SHARED)
add_library(Document::Backends::Pdf ALIAS DocumentBackendPdf)

target_sources(DocumentBackendPdf
    PRIVATE
        src/PdfDocument.cpp
)

target_include_directories(DocumentBackendPdf
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Document/Pdf
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set(PDFium_DIR 3rdparty/pdfium)
find_package(PDFium)

target_link_libraries(DocumentBackendPdf
    PUBLIC
        Document::API
    PRIVATE
        pdfium
)
