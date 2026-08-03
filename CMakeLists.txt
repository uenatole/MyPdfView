cmake_minimum_required(VERSION 4.1)
project(MyPdfView)

set(BUILD_PDF_BACKEND ON CACHE BOOL "" FORCE)
set(BUILD_OCR_BACKEND ON CACHE BOOL "" FORCE)
set(DOCUMENT_VIEW_DRAW_PAGE_LAYOUT_MARKUP ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(Document)
add_subdirectory(DocumentView)

# TODO: move it out
# <
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(example examples/main.cpp)

target_link_libraries(example
        PRIVATE
            DocumentView
            Document::STD
            Document::Backends::Pdf
            Document::Backends::Ocr
            Qt::Widgets
)
# >
