/****************************************************************************
** Meta object code from reading C++ file 'qtgroupboxpropertybrowser.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.2.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "qtpropertybrowser/src/qtgroupboxpropertybrowser.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qtgroupboxpropertybrowser.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.2.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_QtGroupBoxPropertyBrowser_t {
    QByteArrayData data[4];
    char stringdata[59];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    offsetof(qt_meta_stringdata_QtGroupBoxPropertyBrowser_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData) \
    )
static const qt_meta_stringdata_QtGroupBoxPropertyBrowser_t qt_meta_stringdata_QtGroupBoxPropertyBrowser = {
    {
QT_MOC_LITERAL(0, 0, 25),
QT_MOC_LITERAL(1, 26, 10),
QT_MOC_LITERAL(2, 37, 0),
QT_MOC_LITERAL(3, 38, 19)
    },
    "QtGroupBoxPropertyBrowser\0slotUpdate\0"
    "\0slotEditorDestroyed\0"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_QtGroupBoxPropertyBrowser[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   24,    2, 0x08,
       3,    0,   25,    2, 0x08,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void QtGroupBoxPropertyBrowser::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        QtGroupBoxPropertyBrowser *_t = static_cast<QtGroupBoxPropertyBrowser *>(_o);
        switch (_id) {
        case 0: _t->d_func()->slotUpdate(); break;
        case 1: _t->d_func()->slotEditorDestroyed(); break;
        default: ;
        }
    }
    Q_UNUSED(_a);
}

const QMetaObject QtGroupBoxPropertyBrowser::staticMetaObject = {
    { &QtAbstractPropertyBrowser::staticMetaObject, qt_meta_stringdata_QtGroupBoxPropertyBrowser.data,
      qt_meta_data_QtGroupBoxPropertyBrowser,  qt_static_metacall, 0, 0}
};


const QMetaObject *QtGroupBoxPropertyBrowser::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QtGroupBoxPropertyBrowser::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_QtGroupBoxPropertyBrowser.stringdata))
        return static_cast<void*>(const_cast< QtGroupBoxPropertyBrowser*>(this));
    return QtAbstractPropertyBrowser::qt_metacast(_clname);
}

int QtGroupBoxPropertyBrowser::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QtAbstractPropertyBrowser::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 2;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
