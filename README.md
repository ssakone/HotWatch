# QSql

* Configuration CMake
```cmake
add_subdirectory(QSql) # in your project file CMakeFiles.txt
... 
target_link_libraries(... QHotwatch)
```
 - In main cpp

```c++
#include <QSql/include/hotwatch.h>
...
qmlRegisterType<HotWatch>("QSql",1,0,"SqlDriver"); // insert it after engine declaration
```

#   H o t W a t c h  
 