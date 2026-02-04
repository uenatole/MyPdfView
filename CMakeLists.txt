cmake_minimum_required(VERSION 4.1)
project(MyPdfView)

set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 COMPONENTS REQUIRED Pdf Widgets)

add_executable(MyPdfView
        pdf/PdfView.cpp
        pdf/PdfView.h
        main.cpp)

target_link_libraries(MyPdfView PRIVATE Qt::Pdf Qt::Widgets)
