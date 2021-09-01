#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <sstream>

int main(int argc, char *argv[])
{
  if (argc < 1 || argc > 4) {
    std::cerr << "error: invalid arguments" << std::endl;
    return 1;
  }

  int args[3] = {
    330, // loop length
    80,  // interval
    0    // delay
  };

  std::cout << "# Process started with :" << std::endl;
  for (int indx = 1; indx < argc && indx < 4; ++indx) {
    args[indx - 1] = int(*argv[indx]);
    std::cout << "\t Arg " << indx << ":" << args[indx - 1] << std::endl;
  }

  std::stringstream stream;

  // Push dummy messages to stdout
  for (int i = 10; i < int(args[0]); i += int(args[1])) {
    for (int j = 0; j < i; ++j) {
      stream << j << ": This is the test message on stream \n";
    }
    std::this_thread::sleep_for(std::chrono::microseconds(int(args[2])));
    std::cout << i << stream.str() << std::endl;
  }

  std::cout << "# Waiting ..." << std::endl;
  std::cout << "# Doing some task" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(20));
  std::cout << "# Process ending" << std::endl;

  return 0;
}