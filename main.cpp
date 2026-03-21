import std;
import manadrain;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::println(std::cerr, "Usage: {} <filepath>", argv[0]);
    return 1;
  }

  std::string filepath = argv[1];
  if (not std::filesystem::exists(filepath)) {
    std::println(std::cerr, "Error: file does not exist: {}", filepath);
    return 1;
  }

  std::ifstream file{filepath};
  if (not file.is_open()) throw std::runtime_error{
    std::format("could not open file: {}", filepath)
  };

  Manadrain::Parser parser = Manadrain::Parser{
    .source_str = std::ranges::to<std::string>(
      std::ranges::subrange(
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{}
      )
    )
  };
  parser.Parse();
  
  return 0;
}
