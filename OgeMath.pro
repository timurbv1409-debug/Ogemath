QT += core gui widgets network charts
CONFIG += c++17
TEMPLATE = app
TARGET = OgeMath

SOURCES += \
    calendarpage.cpp \
    main.cpp \
    mainwindow.cpp \
    examselectwindow.cpp \
    progresspage.cpp

HEADERS += \
    calendarpage.h \
    mainwindow.h \
    examselectwindow.h \
    progresspage.h

DISTFILES += \
    build/Desktop_Qt_6_10_2_MinGW_64_bit-Debug/debug/data/catalog.json \
    build/Desktop_Qt_6_10_2_MinGW_64_bit-Debug/debug/data/plan.json \
    build/Desktop_Qt_6_10_2_MinGW_64_bit-Debug/debug/data/progress.json \
    build/Desktop_Qt_6_10_2_MinGW_64_bit-Debug/debug/data/sessions.json \
    build/Desktop_Qt_6_10_2_MinGW_64_bit-Debug/debug/data/submissions.json \
    data/catalog.json \
    data/progress.json \
    data/sessions.json \
    data/submissions.json \
    data/plan.json \
    data/variants/variant_01.json \
    data/variants/variant_02.json \
    data/variants/variant_03.json \
    data/variants/variant_04.json \
    data/variants/variant_05.json \
    data/variants/variant_06.json \
    data/variants/variant_07.json \
    data/variants/variant_08.json \
    data/variants/variant_09.json \
    data/variants/variant_10.json \
    data/variants/variant_11.json \
    data/variants/variant_12.json \
    data/variants/variant_13.json \
    data/variants/variant_14.json \
    data/variants/variant_15.json \
    data/variants/variant_16.json \
    data/variants/variant_17.json \
    data/variants/variant_18.json \
    data/variants/variant_19.json \
    data/variants/variant_20.json \
    data/variants/variant_21.json \
    data/variants/variant_22.json \
    data/variants/variant_23.json \
    data/variants/variant_24.json \
    data/variants/variant_25.json \
    data/variants/variant_26.json \
    data/variants/variant_27.json \
    data/variants/variant_28.json \
    data/variants/variant_29.json \
    data/variants/variant_30.json \
    data/variants/variant_31.json \
    data/variants/variant_32.json \
    data/variants/variant_33.json \
    data/variants/variant_34.json \
    data/variants/variant_35.json \
    data/variants/variant_36.json

RESOURCES += \
    resources.qrc
