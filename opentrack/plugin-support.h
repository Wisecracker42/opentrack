#pragma once

#include "plugin-api.hpp"

#include <QWidget>
#include <QDebug>
#include <QString>
#include <QLibrary>
#include <QFrame>
#include <QList>

#include <memory>
template<typename t> using ptr = std::shared_ptr<t>;

extern "C" typedef void* (*CTOR_FUNPTR)(void);
extern "C" typedef Metadata* (*METADATA_FUNPTR)(void);

struct dylib {
    enum Type { Filter, Tracker, Protocol };
    
    dylib(const QString& filename, Type t);
    ~dylib();
    static QList<ptr<dylib>> enum_libraries();
    
    Type type;
    QString filename;
    
    QIcon icon;
    QString name;
    
    CTOR_FUNPTR Dialog;
    CTOR_FUNPTR Constructor;
    METADATA_FUNPTR Meta;
private:
#if defined(_WIN32)
    QLibrary* handle;
#else
    void* handle;
#endif
};

struct SelectedLibraries {
    using dylibtr = ptr<dylib>;
    ptr<ITracker> pTracker;
    ptr<IFilter> pFilter;
    ptr<IProtocol> pProtocol;
    SelectedLibraries(QFrame* frame, dylibtr t, dylibtr p, dylibtr f);
    SelectedLibraries() : pTracker(nullptr), pFilter(nullptr), pProtocol(nullptr), correct(false) {}
    ~SelectedLibraries();
    bool correct;
};