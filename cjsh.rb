class Cjsh < Formula
  desc "CJ's Shell"
  homepage "https://github.com/CadenFinley/CJsShell"
  url "https://github.com/CadenFinley/CJsShell/archive/refs/tags/v2.0.2.3.tar.gz"
  sha256 "8316487785961b25ca58e868fcbfc83dfe3babf6456320db6de1634ba637fe92"
  license "MIT"

  depends_on "cmake" => :build
  depends_on "openssl@3"
  depends_on "pkg-config" => :build

  def install
    mkdir "build" do
      system "cmake", "..",
             "-DCMAKE_BUILD_TYPE=Release",
             "-DCMAKE_INSTALL_PREFIX=#{prefix}",
             "-DCMAKE_PREFIX_PATH=#{Formula["openssl@3"].opt_prefix}",
             *std_cmake_args
      system "make", "install"
    end
  end

  test do
    assert_match "v#{version}", shell_output("#{bin}/cjsh --version")
  end
end