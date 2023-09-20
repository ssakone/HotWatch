#ifndef HOTWATCH_H
#define HOTWATCH_H

#include <QtCore>
#include <QtQml>
#include "config.h"

class HotWatch : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void setup();
    Q_INVOKABLE QString sourceFile();

    explicit HotWatch(QQmlEngine *engine, QObject *parent = nullptr);
    static void registerSingleton();

signals:
    void fileChange(QString path);

private:
    QQmlEngine *_engine;
    QStringList _defaultImportPaths;

};

#endif // HOTWATCH_H
