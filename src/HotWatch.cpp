#include "HotWatch.h"
#include "QtQml/qqml.h"

HotWatch::HotWatch(QQmlEngine *engine, QObject *parent)
    : QObject{parent}, _engine{engine} {}


void HotWatch::clearCache() { _engine->clearComponentCache(); }

void HotWatch::setup() {
  qDebug() << "[HotWatch]" << "Settings...";
  QFileSystemWatcher *watcher = new QFileSystemWatcher(this);
  connect(watcher, &QFileSystemWatcher::fileChanged, this, [=](QString path) {
    watcher->addPath(path);
    this->clearCache();
    emit this->fileChange(path);
  });
#if defined(Q_OS_LINUX)
  QDirIterator it(QString(APP_SOURCE_DIR),
                  QStringList() << "*.qml",
                  QDir::Files,
                  QDirIterator::Subdirectories);
#else
  QDirIterator it(QString(APP_SOURCE_DIR),
                  QStringList() << "*.qml",
                  QDir::Files,
                  QDirIterator::Subdirectories);
#endif
  while (it.hasNext()) {
    watcher->addPath(it.next());
  }
}

QString HotWatch::sourceFile()
{
  return APP_SOURCE_DIR;
}


//void registerTypes() { qmlRegisterType<HotWatch>("HotWatchBackend", 1, 0, "HotWatcher"); }

 void HotWatch::registerSingleton() {
   qmlRegisterSingletonType<HotWatch>(
       "HotWatchBackend", 1, 0, "HotWatcher",
       [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
         Q_UNUSED(scriptEngine);
         auto *instance = new HotWatch(engine, engine);
         instance->_defaultImportPaths = engine->importPathList();
         return instance;
       });
 }

 //Q_COREAPP_STARTUP_FUNCTION(HotWatch::registerSingleton);
