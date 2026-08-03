find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(QtDocumentViewer
    rubrication/DocumentRubricationModel.cpp
    rubrication/DocumentRubricationModel.h
    main.cpp
)

target_link_libraries(QtDocumentViewer
    PRIVATE
        QtDocumentView
        Document::STD
        Document::Backends::Pdf
        Document::Backends::Ocr
        Qt::Widgets
)
