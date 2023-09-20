# HotWatch


* Configuration CMake


```cmake
add_subdirectory(HotWatch) # in your project file CMakeFiles.txt
... 

target_link_libraries(...
	HotWatch
	HotWatchplugin
)
```
- In main cpp

```c++

#include <HotWatch.h>

...

HotWatch::registerSingleton(); // insert it after engine declaration

```
