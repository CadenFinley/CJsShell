#include <gtest/gtest.h>
#include "parser.h"
#include "main.h"

// Mock global variable needed by parser.cpp
bool g_debug_mode = false;

// simple tokenize test
TEST(TokenizeCommand, SplitsOnSpaces) {
  auto t = tokenize_command("echo hello world");
  EXPECT_EQ(t.size(), 3);
  EXPECT_EQ(t[0], "echo");
  EXPECT_EQ(t[1], "hello");
  EXPECT_EQ(t[2], "world");
}

// quotes and escapes
TEST(TokenizeCommand, HandlesQuotes) {
  auto t = tokenize_command(R"(say "a b" 'c d')"); 
  std::vector<std::string> expect = {"say","a b","c d"};
  EXPECT_EQ(t, expect);
}

// env var expansion
TEST(ExpandEnvVars, ReplacesKnown) {
  Parser parser;
  std::string s = "$HOME/test";
  parser.expand_env_vars(s);
  const char* h = getenv("HOME");
  ASSERT_TRUE(h);
  EXPECT_EQ(s, std::string(h) + "/test");
}

// escape sequences
TEST(TokenizeCommand, HandlesEscapedSpaces) {
  auto t = tokenize_command(R"(arg\ with\ spaces)");
  ASSERT_EQ(t.size(), 1);
  EXPECT_EQ(t[0], "arg with spaces");
}

// nested quotes and escapes
TEST(TokenizeCommand, ComplexQuotesAndEscapes) {
  auto t = tokenize_command(R"(echo "He said \"Hello\" and left")");
  ASSERT_EQ(t.size(), 2);
  EXPECT_EQ(t[0], "echo");
  EXPECT_EQ(t[1], "He said \"Hello\" and left");
}

// brace expansion
TEST(ParseCommand, BraceExpansion) {
  Parser parser;
  auto t = parser.parse_command("echo {a,b,c}");
  std::vector<std::string> exp = {"echo","a","b","c"};
  EXPECT_EQ(t, exp);
}

// nested brace expansion
TEST(ParseCommand, NestedBraces) {
  Parser parser;
  auto t = parser.parse_command("cmd {x,{y,z}} end");
  std::vector<std::string> exp = {"cmd","x","y","z","end"};
  EXPECT_EQ(t, exp);
}

// tilde expansion
TEST(ParseCommand, TildeExpansion) {
  Parser parser;
  setenv("HOME", "/home/testuser", 1);
  auto t = parser.parse_command("echo ~ ~/docs");
  ASSERT_EQ(t.size(), 3);
  EXPECT_EQ(t[1], "/home/testuser");
  EXPECT_EQ(t[2], "/home/testuser/docs");
  unsetenv("HOME");
}

// pipeline parsing with redirection and background
TEST(ParsePipeline, RedirectionAndBackground) {
  Parser parser;
  auto cmds = parser.parse_pipeline("cat < in.txt | grep foo > out.txt >> app.txt &");
  ASSERT_EQ(cmds.size(), 2);
  // first command
  EXPECT_EQ(cmds[0].args[0], "cat");
  EXPECT_EQ(cmds[0].input_file, "in.txt");
  EXPECT_FALSE(cmds[0].background);
  // second command
  EXPECT_EQ(cmds[1].args[0], "grep");
  EXPECT_EQ(cmds[1].output_file, "out.txt");
  EXPECT_EQ(cmds[1].append_file, "app.txt");
  EXPECT_TRUE(cmds[1].background);
}

// semicolon command parsing
TEST(ParseSemicolonCommands, MultipleCommands) {
  Parser parser;
  auto cmds = parser.parse_semicolon_commands(" echo a ; echo b;echo c ");
  std::vector<std::string> exp = {"echo a","echo b","echo c"};
  EXPECT_EQ(cmds, exp);
}

// logical command parsing
TEST(ParseLogicalCommands, AndOrOperators) {
  Parser parser;
  auto lcmds = parser.parse_logical_commands("cmd1 && cmd2||cmd3 &&cmd4");
  ASSERT_EQ(lcmds.size(), 4);
  EXPECT_EQ(lcmds[0].command, "cmd1 ");
  EXPECT_EQ(lcmds[0].op, "&&");
  EXPECT_EQ(lcmds[1].command, " cmd2");
  EXPECT_EQ(lcmds[1].op, "||");
  EXPECT_EQ(lcmds[2].command, "cmd3 ");
  EXPECT_EQ(lcmds[2].op, "&&");
  EXPECT_EQ(lcmds[3].command, "cmd4");
  EXPECT_EQ(lcmds[3].op, "");
}

// environment assignment detection
TEST(IsEnvAssignment, ValidAndInvalid) {
  Parser parser;
  std::string name, value;
  EXPECT_TRUE(parser.is_env_assignment("VAR1=hello", name, value));
  EXPECT_EQ(name, "VAR1");
  EXPECT_EQ(value, "hello");

  EXPECT_TRUE(parser.is_env_assignment("X_Y=123", name, value));
  EXPECT_EQ(name, "X_Y");
  EXPECT_EQ(value, "123");

  EXPECT_TRUE(parser.is_env_assignment("NOVALUE=", name, value));
  EXPECT_EQ(name, "NOVALUE");
  EXPECT_EQ(value, "");
  
  EXPECT_FALSE(parser.is_env_assignment("1INVALID=foo", name, value));
}

// Advanced tokenization tests
TEST(TokenizeCommand, HandlesEmptyStrings) {
  auto t = tokenize_command("");
  EXPECT_EQ(t.size(), 0);
}

TEST(TokenizeCommand, HandlesMultipleSpacesAndTabs) {
  auto t = tokenize_command("cmd  arg1\t\targ2   arg3");
  ASSERT_EQ(t.size(), 4);
  EXPECT_EQ(t[0], "cmd");
  EXPECT_EQ(t[1], "arg1");
  EXPECT_EQ(t[2], "arg2");
  EXPECT_EQ(t[3], "arg3");
}

TEST(TokenizeCommand, MixedQuoteTypes) {
  auto t = tokenize_command(R"(echo "double 'quoted'" 'single "quoted"')");
  ASSERT_EQ(t.size(), 3);
  EXPECT_EQ(t[0], "echo");
  EXPECT_EQ(t[1], "double 'quoted'");
  EXPECT_EQ(t[2], "single \"quoted\"");
}

TEST(TokenizeCommand, BackslashAtEnd) {
  auto t = tokenize_command(R"(echo test\ )");
  ASSERT_EQ(t.size(), 2);
  EXPECT_EQ(t[0], "echo");
  EXPECT_EQ(t[1], "test ");
}

// Advanced environment variable tests
TEST(ExpandEnvVars, MultipleVariables) {
  Parser parser;
  setenv("VAR1", "hello", 1);
  setenv("VAR2", "world", 1);
  std::string s = "$VAR1 $VAR2";
  parser.expand_env_vars(s);
  EXPECT_EQ(s, "hello world");
  unsetenv("VAR1");
  unsetenv("VAR2");
}

TEST(ExpandEnvVars, VariableWithDefault) {
  Parser parser;
  setenv("EXISTING", "value", 1);
  std::string s1 = "${EXISTING:-default}";
  std::string s2 = "${NONEXISTING:-default}";
  parser.expand_env_vars(s1);
  parser.expand_env_vars(s2);
  EXPECT_EQ(s1, "value");
  EXPECT_EQ(s2, "default");
  unsetenv("EXISTING");
}

TEST(ExpandEnvVars, QuotedVariables) {
  Parser parser;
  setenv("VAR", "value", 1);
  std::string s1 = R"("$VAR")";
  std::string s2 = R"('$VAR')";
  parser.expand_env_vars(s1);
  parser.expand_env_vars(s2);
  EXPECT_EQ(s1, R"("value")");
  EXPECT_EQ(s2, R"('$VAR')"); // Variables in single quotes shouldn't expand
  unsetenv("VAR");
}

// Brace expansion tests
TEST(ParseCommand, NumericBraceExpansion) {
  Parser parser;
  auto t = parser.parse_command("echo {1..5}");
  std::vector<std::string> exp = {"echo", "1", "2", "3", "4", "5"};
  EXPECT_EQ(t, exp);
}

TEST(ParseCommand, SteppedBraceExpansion) {
  Parser parser;
  auto t = parser.parse_command("echo {1..10..2}");
  std::vector<std::string> exp = {"echo", "1", "3", "5", "7", "9"};
  EXPECT_EQ(t, exp);
}

TEST(ParseCommand, AlphabeticBraceExpansion) {
  Parser parser;
  auto t = parser.parse_command("echo {a..e}");
  std::vector<std::string> exp = {"echo", "a", "b", "c", "d", "e"};
  EXPECT_EQ(t, exp);
}

TEST(ParseCommand, MultipleBraceExpansions) {
  Parser parser;
  auto t = parser.parse_command("echo {a,b}{1,2}");
  std::vector<std::string> exp = {"echo", "a1", "a2", "b1", "b2"};
  EXPECT_EQ(t, exp);
}

// Pathname expansion (globbing)
TEST(ParseCommand, BasicGlob) {
  Parser parser;
  // This test assumes certain files exist - use mock filesystem or create temp files
  // auto t = parser.parse_command("echo *.txt");
  // EXPECT_GT(t.size(), 1); // At least "echo" plus matching files
}

// Command substitution
TEST(ParseCommand, CommandSubstitution) {
  Parser parser;
  auto t = parser.parse_command("echo $(echo hello)");
  std::vector<std::string> exp = {"echo", "hello"};
  EXPECT_EQ(t, exp);
}

TEST(ParseCommand, NestedCommandSubstitution) {
  Parser parser;
  auto t = parser.parse_command("echo $(echo $(echo nested))");
  std::vector<std::string> exp = {"echo", "nested"};
  EXPECT_EQ(t, exp);
}

// Arithmetic expansion
TEST(ParseCommand, ArithmeticExpansion) {
  Parser parser;
  auto t = parser.parse_command("echo $((2 + 3))");
  std::vector<std::string> exp = {"echo", "5"};
  EXPECT_EQ(t, exp);
}

// Complex redirection
TEST(ParsePipeline, ComplexRedirection) {
  Parser parser;
  auto cmds = parser.parse_pipeline("cmd 2>&1 1>/dev/null <input.txt");
  ASSERT_EQ(cmds.size(), 1);
  EXPECT_EQ(cmds[0].args[0], "cmd");
  EXPECT_EQ(cmds[0].input_file, "input.txt");
  EXPECT_EQ(cmds[0].output_file, "/dev/null");
  EXPECT_TRUE(cmds[0].stderr_to_stdout);
}

// Here-document parsing
TEST(ParsePipeline, HereDocument) {
  Parser parser;
  auto cmds = parser.parse_pipeline("cat << EOF\nline1\nline2\nEOF");
  ASSERT_EQ(cmds.size(), 1);
  EXPECT_EQ(cmds[0].args[0], "cat");
  EXPECT_EQ(cmds[0].here_doc, "line1\nline2\n");
}

// Error handling
TEST(ParseCommand, UnterminatedQuote) {
  Parser parser;
  EXPECT_THROW({
    parser.parse_command("echo \"unterminated");
  }, std::runtime_error);
}

TEST(ParseCommand, UnmatchedBrace) {
  Parser parser;
  EXPECT_THROW({
    parser.parse_command("echo {a,b");
  }, std::runtime_error);
}

// Complex pipelines and logical operators
TEST(ParsePipeline, ComplexPipeline) {
  Parser parser;
  auto cmds = parser.parse_pipeline("grep pattern file | sort -r | uniq -c | head -5");
  ASSERT_EQ(cmds.size(), 4);
  EXPECT_EQ(cmds[0].args[0], "grep");
  EXPECT_EQ(cmds[1].args[0], "sort");
  EXPECT_EQ(cmds[2].args[0], "uniq");
  EXPECT_EQ(cmds[3].args[0], "head");
}

TEST(ParseLogicalCommands, ComplexLogicalSequence) {
  Parser parser;
  auto lcmds = parser.parse_logical_commands("cmd1 && (cmd2 || cmd3) && cmd4");
  // This test assumes your parser handles parentheses grouping
  ASSERT_GE(lcmds.size(), 3);
  EXPECT_EQ(lcmds[0].command, "cmd1 ");
  EXPECT_EQ(lcmds[0].op, "&&");
}
