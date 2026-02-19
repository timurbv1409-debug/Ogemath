QT       += core gui
QT += widgets network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    examselectwindow.cpp \
    main.cpp \
    mainwindow.cpp \
    progresspage.cpp \
    progressstore.cpp

HEADERS += \
    examselectwindow.h \
    mainwindow.h \
    progresspage.h \
    progressstore.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    data/progress.json \
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
