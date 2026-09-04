#include <iostream>

int RunRingBufferTests();
int RunTranscriptTests();
int RunAudioFormatTests();
int RunVersionTests();

int main() {
  int failures = 0;
  failures += RunRingBufferTests();
  failures += RunTranscriptTests();
  failures += RunAudioFormatTests();
  failures += RunVersionTests();
  if (failures == 0) {
    std::cout << "All tests passed\n";
    return 0;
  }
  std::cerr << failures << " assertion(s) failed\n";
  return 1;
}
