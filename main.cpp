import manadrain;
import std;

std::string source_str(std::string filepath) {
  std::ifstream file{filepath};
  if (not file.is_open()) throw std::runtime_error{
    std::format("could not open file: {}", filepath)
  };

  return std::ranges::to<std::string>(
    std::ranges::subrange(
      std::istreambuf_iterator<char>{file},
      std::istreambuf_iterator<char>{}
    )
  );
}

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

  {
    using namespace JS;
    std::shared_ptr rt = std::make_shared<Runtime>();

    std::shared_ptr ctx = NewContext(rt);
    EvalInternal(
      ctx, Unit::TAG_NULL, source_str(filepath), "", 0
    );
  }
  
  return 0;
}

