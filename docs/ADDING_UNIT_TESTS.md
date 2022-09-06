# Adding Unit Tests

## Writing Unit tests with CMake

Unit tests using the googletest framework compile into a standalone test
executable that uses a simple gtest-provided main() as the entry point, and
which links against a library of code that is to be tested (the Unit Under
Test, or UUT). The unit tests then exercise functions and methods within the
UUT.

## Refactoring an existing app with a main()

If the UUT is an application then it will already have a main() entry point
which will not invoke gtest unless modified or substituted. Whilst some guides
show the application main() being modified to run tests if a mode switch is
set, I prefer to keep the test code completely separate so that the unit that
is being tested is exactly the same as that built for production.

I will assume that the app is called `MyApp` and that all of the source is at
the root of the project. Additionally I'm assuming that there is no test code
there yet. This is what you have to do:

### Split the app into a small executable and a library

* Rename the existing `main()` function to `MyAppMain()`
* Add a suitable prototype for MyAppMain() into MyApp.h (which probably already
exists).

* Add a new file called `MyAppMain.cpp`
* Add the following code to `MyAppMain.cpp`:

      #include "CrossPlatform.h"  // Required for that nasty TCHAR stuff
      #include "MyApp.h"

      #include "LibHeader.h"

      int main(int argc, TCHAR* argv [], TCHAR* envp [])
      {
          MyAppMain(argc, argv, envp);
      }

### Create a test folder

All of the test code will live in a folder called `test` under the root of this
project. This folder will contain the unit test source code (in this example
`UnitTest.cpp`).
I have adopted a convention that names all unit tests as `<TestName>UnitTests`
which allows them to be easily identified and executed together by
`ctest -R UnitTests`

* Write a basic unit test that is guaranteed to fail:

      #include "gtest/gtest.h"

      TEST(MyAppUnitTests, UnitTest1) {
         EXPECT_TRUE(false);
      }

* Add a `CMakeLists.txt` file to the test folder with content as in this
 example:

      # Create the test executable by combining the unit test code with gtest_main
      add_executable(MyAppUnitTests UnitTest.cpp)

      # If the standard runtime is not MTDLL in Windows then you'll need this too
      set_property(TARGET MyAppUnitTests PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

      # Link it with the library code under test
      target_link_libraries(MyAppUnitTests MyAppLib gtest_main)

       # Register this application as a test in the system so that ctest can
        find it
       add_test(MyAppUnitTests MyAppUnitTests)

## Update the top-level CMakeLists.txt file

* Recreate the executable using `add_executable(MyApp MyAppMain.cpp)`
* Use `target_link_libraries(MyApp MyAppLib)` to bring in the library
containing the remainder of the application code
* You may need to `target_add_include_directories(MyAppLib PUBLIC .)` otherwise
the test code will not see the headers.

* Add `enable_testing()`. This then applies recursively throughout the project.
* Include the unit test code with an `add_subdirectory(test)`. Note that this
must be added after any language selection properties are set.

* Need to ensure that the same runtime library is used for all components that
are linked together. For example

      set_property(TARGET TestLib PROPERTY
           MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

## Building and running

Employ the usual sequence [described here](../README.md) to build and run.
