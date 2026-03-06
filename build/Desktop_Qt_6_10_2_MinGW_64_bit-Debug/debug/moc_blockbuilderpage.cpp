/****************************************************************************
** Meta object code from reading C++ file 'blockbuilderpage.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../blockbuilderpage.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'blockbuilderpage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN16BlockBuilderPageE_t {};
} // unnamed namespace

template <> constexpr inline auto BlockBuilderPage::qt_create_metaobjectdata<qt_meta_tag_ZN16BlockBuilderPageE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "BlockBuilderPage",
        "backRequested",
        "",
        "trainingStarted",
        "QList<TrainingStateService::Block>",
        "blocks",
        "modeTitle",
        "plannedMode",
        "onAddBlock",
        "onRemoveSelected",
        "onSelectionChanged",
        "onStartClicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'backRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'trainingStarted'
        QtMocHelpers::SignalData<void(const QVector<TrainingStateService::Block> &, const QString &, bool)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 }, { QMetaType::QString, 6 }, { QMetaType::Bool, 7 },
        }}),
        // Slot 'onAddBlock'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRemoveSelected'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSelectionChanged'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onStartClicked'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<BlockBuilderPage, qt_meta_tag_ZN16BlockBuilderPageE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject BlockBuilderPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16BlockBuilderPageE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16BlockBuilderPageE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16BlockBuilderPageE_t>.metaTypes,
    nullptr
} };

void BlockBuilderPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<BlockBuilderPage *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->backRequested(); break;
        case 1: _t->trainingStarted((*reinterpret_cast<std::add_pointer_t<QList<TrainingStateService::Block>>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 2: _t->onAddBlock(); break;
        case 3: _t->onRemoveSelected(); break;
        case 4: _t->onSelectionChanged(); break;
        case 5: _t->onStartClicked(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (BlockBuilderPage::*)()>(_a, &BlockBuilderPage::backRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (BlockBuilderPage::*)(const QVector<TrainingStateService::Block> & , const QString & , bool )>(_a, &BlockBuilderPage::trainingStarted, 1))
            return;
    }
}

const QMetaObject *BlockBuilderPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *BlockBuilderPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16BlockBuilderPageE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int BlockBuilderPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void BlockBuilderPage::backRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void BlockBuilderPage::trainingStarted(const QVector<TrainingStateService::Block> & _t1, const QString & _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
