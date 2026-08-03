cmake_minimum_required(VERSION 4.1)
project(MyPdfView)

set(BUILD_PDF_BACKEND ON CACHE BOOL "" FORCE)
set(BUILD_OCR_BACKEND ON CACHE BOOL "" FORCE)
set(DOCUMENT_VIEW_DRAW_PAGE_LAYOUT_MARKUP OFF CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(Document)
add_subdirectory(QtDocumentView)
add_subdirectory(QtDocumentViewer)
