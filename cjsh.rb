class Cjsh < Formula
  desc "CJ's Shell"
  homepage "https://github.com/CadenFinley/CJsShell"
  url "https://github.com/CadenFinley/CJsShell.git",
      tag:      "v2.0.2.3",
      revision: "INSERT_COMMIT_SHA"
  version "2.0.2.3"

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