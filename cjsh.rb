class Cjsh < Formula
  desc "CJ's Shell"
  homepage "https://github.com/CadenFinley/CJsShell"
  url "https://github.com/CadenFinley/CJsShell.git",
      tag:      "2.0.2.3",
      revision: "b51b00509aa926a7f7961b4ec08c0d5ab8f06873bae0870625d60615c22891d3"
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

  def post_install
    original_shell = ENV["SHELL"]
    (prefix/"original_shell.txt").write original_shell
  end

  def post_uninstall
    original = (prefix/"original_shell.txt").read.chomp rescue nil
    return if original.to_s.empty?
    ohai "Restoring your original shell to #{original}"
    safe_system "sudo", "chsh", "-s", original, ENV["USER"]
  end

  test do
    assert_match "v#{version}", shell_output("#{bin}/cjsh --version")
  end
end