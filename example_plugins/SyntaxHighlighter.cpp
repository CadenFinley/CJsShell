#include "plugininterface.h"
#include <iostream>
#include <streambuf>
#include <vector>
#include <map>
#include <queue>
#include <string>
#include <sstream>
#include <regex>

// Define color codes for syntax highlighting
struct SyntaxColors {
    std::string keyword = "\033[1;36m";   // Cyan bold
    std::string string = "\033[32m";       // Green
    std::string number = "\033[33m";       // Yellow
    std::string comment = "\033[2;37m";    // Gray
    std::string function = "\033[1;34m";   // Blue bold
    std::string normal = "\033[0m";        // Reset
};

// Fixed definition of CustomCoutBuffer class
class CustomCoutBuffer : public std::streambuf {  // Removed extra dot
    std::streambuf* original;
    bool enabled;
    
    // Buffer to store the entire output
    std::stringstream outputBuffer;
    SyntaxColors syntaxColors;

protected:
    int overflow(int c) {
        if (!enabled) {
            return original->sputc(c);
        }
        
        // Add character to our buffer
        outputBuffer.put(c);
        return c;
    }
    
    int sync() {
        if (!enabled) {
            // Simply redirect to original buffer
            std::string content = outputBuffer.str();
            for (char c : content) {
                original->sputc(c);
            }
        } else {
            // Process the entire buffer for code blocks
            std::string content = outputBuffer.str();
            std::string processedContent = processContent(content);
            
            // Output the processed content
            for (char c : processedContent) {
                original->sputc(c);
            }
        }
        
        // Clear the buffer and sync the original stream
        outputBuffer.str("");
        outputBuffer.clear();
        original->pubsync();
        return 0;
    }
    
    std::string processContent(const std::string& content) {
        std::stringstream result;
        std::istringstream input(content);
        std::string line;
        bool inCodeBlock = false;
        std::string language;
        
        while (std::getline(input, line)) {
            // Check for code block start: ```language
            if (!inCodeBlock && line.length() >= 3 && line.substr(0, 3) == "```") {
                inCodeBlock = true;
                if (line.length() > 3) {
                    language = line.substr(3);
                } else {
                    language = "";
                }
                result << line << "\n";
                continue;
            }
            
            // Check for code block end: ```
            if (inCodeBlock && line == "```") {
                inCodeBlock = false;
                language = "";
                result << line << "\n";
                continue;
            }
            
            // If in code block, apply syntax highlighting
            if (inCodeBlock) {
                result << applySyntaxHighlighting(line, language) << "\n";
            } else {
                result << line << "\n";
            }
        }
        
        // Remove the last newline if the original content didn't end with one
        std::string finalResult = result.str();
        if (!content.empty() && content.back() != '\n' && !finalResult.empty()) {
            finalResult.pop_back();
        }
        
        return finalResult;
    }
    
    std::string applySyntaxHighlighting(const std::string& line, const std::string& language) {
        std::string result = line;
        std::string lang = language;
        
        // Normalize language name
        std::transform(lang.begin(), lang.end(), lang.begin(), ::tolower);
        
        // Standard keywords across many languages
        std::vector<std::string> commonKeywords = {"if", "else", "for", "while", "return", "break", "continue"};
        
        // Generic number regex that matches integers, decimals, scientific notation, and hex
        std::regex numberRegex("\\b(0[xX][0-9a-fA-F]+|[0-9]+(?:\\.[0-9]*)?(?:[eE][+-]?[0-9]+)?)\\b");
        
        // Apply language-specific syntax highlighting
        if (lang == "python" || lang == "py") {
            // Python keywords
            std::vector<std::string> keywords = {"def", "class", "elif", "import", "from", "as", "try", "except", 
                "finally", "with", "in", "is", "and", "or", "not", "pass", "lambda", "nonlocal", "global", 
                "async", "await", "yield", "assert", "del", "raise"};
            keywords.insert(keywords.end(), commonKeywords.begin(), commonKeywords.end());
            
            // Python numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Python strings
            std::regex stringRegex("(['\"])(.*?)\\1");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // Python comments
            std::regex commentRegex("#.*$");
            result = std::regex_replace(result, commentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // Python functions
            std::regex funcRegex("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
            result = std::regex_replace(result, funcRegex, syntaxColors.function + "$1" + syntaxColors.normal + "(");
            
            // Apply keywords at the end to avoid interference with function names
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        else if (lang == "cpp" || lang == "c++" || lang == "c") {
            // C++ keywords
            std::vector<std::string> keywords = {"auto", "case", "class", "const", "default", "do", "enum", 
                "extern", "goto", "register", "sizeof", "static", "struct", "switch", "typedef", "union", 
                "volatile", "new", "delete", "try", "catch", "throw", "namespace", "using", "template", 
                "virtual", "friend", "public", "private", "protected", "inline", "explicit", "typename",
                "constexpr", "override", "final", "nullptr", "noexcept", "decltype", "mutable", "operator"};
            keywords.insert(keywords.end(), commonKeywords.begin(), commonKeywords.end());
            
            // C++ types
            std::vector<std::string> types = {"int", "float", "double", "char", "void", "bool", "short", 
                "long", "unsigned", "signed", "size_t", "wchar_t", "auto", "string", "vector", "map"};
            
            // C++ values
            std::vector<std::string> values = {"true", "false", "NULL", "nullptr"};
            
            // C++ numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // C++ strings
            std::regex stringRegex("(['\"])(.*?)\\1");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // C++ comments
            std::regex lineCommentRegex("//.*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // Functions
            std::regex funcRegex("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
            result = std::regex_replace(result, funcRegex, syntaxColors.function + "$1" + syntaxColors.normal + "(");
            
            // Apply keywords
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
            
            // Apply types
            for (const auto& type : types) {
                std::regex typeRegex("\\b" + type + "\\b");
                result = std::regex_replace(result, typeRegex, syntaxColors.keyword + type + syntaxColors.normal);
            }
            
            // Apply values
            for (const auto& value : values) {
                std::regex valueRegex("\\b" + value + "\\b");
                result = std::regex_replace(result, valueRegex, syntaxColors.number + value + syntaxColors.normal);
            }
        }
        else if (lang == "java") {
            // Java keywords
            std::vector<std::string> keywords = {"abstract", "assert", "boolean", "byte", "case", "catch", "char", 
                "class", "const", "default", "do", "double", "enum", "extends", "final", "finally", "float", 
                "implements", "import", "instanceof", "int", "interface", "long", "native", "new", "package", 
                "private", "protected", "public", "short", "static", "strictfp", "super", "switch", 
                "synchronized", "this", "throw", "throws", "transient", "try", "void", "volatile"};
            keywords.insert(keywords.end(), commonKeywords.begin(), commonKeywords.end());
            
            // Java numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Java values
            std::vector<std::string> values = {"true", "false", "null"};
            
            // Java strings
            std::regex stringRegex("\".*?\"");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // Java comments
            std::regex lineCommentRegex("//.*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // Functions
            std::regex funcRegex("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
            result = std::regex_replace(result, funcRegex, syntaxColors.function + "$1" + syntaxColors.normal + "(");
            
            // Apply keywords
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
            
            // Apply values
            for (const auto& value : values) {
                std::regex valueRegex("\\b" + value + "\\b");
                result = std::regex_replace(result, valueRegex, syntaxColors.number + value + syntaxColors.normal);
            }
        }
        else if (lang == "javascript" || lang == "js" || lang == "ts" || lang == "typescript") {
            // JavaScript/TypeScript keywords
            std::vector<std::string> keywords = {"var", "let", "const", "function", "class", "extends", 
                "import", "export", "from", "as", "async", "await", "try", "catch", "finally", "throw", 
                "typeof", "instanceof", "new", "this", "super", "delete", "in", "of", "do", "case", 
                "switch", "default", "void", "with", "yield"};
            keywords.insert(keywords.end(), commonKeywords.begin(), commonKeywords.end());
            
            // TypeScript specific
            if (lang == "ts" || lang == "typescript") {
                std::vector<std::string> tsKeywords = {"interface", "namespace", "module", "enum", "type", 
                    "implements", "any", "string", "number", "boolean", "public", "private", "protected", 
                    "readonly", "abstract", "declare", "keyof", "never", "unknown"};
                keywords.insert(keywords.end(), tsKeywords.begin(), tsKeywords.end());
            }
            
            // JS/TS numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // JS/TS values
            std::vector<std::string> values = {"true", "false", "null", "undefined", "NaN", "Infinity"};
            
            // JS/TS strings
            std::regex stringRegex("(['\"`])(.*?)\\1");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // JS/TS comments
            std::regex lineCommentRegex("//.*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // Functions
            std::regex funcRegex("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
            result = std::regex_replace(result, funcRegex, syntaxColors.function + "$1" + syntaxColors.normal + "(");
            
            // Apply keywords
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
            
            // Apply values
            for (const auto& value : values) {
                std::regex valueRegex("\\b" + value + "\\b");
                result = std::regex_replace(result, valueRegex, syntaxColors.number + value + syntaxColors.normal);
            }
        }
        else if (lang == "html" || lang == "xml") {
            // HTML/XML tags - use regex_search instead of regex_replace with lambda
            std::regex tagRegex("</?([a-zA-Z][a-zA-Z0-9_:-]*)[^>]*>");
            std::string processed;
            std::string::const_iterator searchStart(result.cbegin());
            std::smatch tagMatch;
            
            // Process all tag matches
            while (std::regex_search(searchStart, result.cend(), tagMatch, tagRegex)) {
                // Add the text before the match
                processed.append(searchStart, tagMatch[0].first);
                
                // Extract components and add with highlighting
                std::string fullTag = tagMatch[0].str();
                std::string tagName = tagMatch[1].str();
                
                // Find the position of the tag name within the full tag
                size_t tagPos = fullTag.find(tagName);
                
                // Add the part before the tag name
                processed.append(fullTag.substr(0, tagPos));
                
                // Add the highlighted tag name
                processed.append(syntaxColors.keyword + tagName + syntaxColors.normal);
                
                // Add the part after the tag name
                processed.append(fullTag.substr(tagPos + tagName.length()));
                
                // Move forward
                searchStart = tagMatch[0].second;
            }
            
            // Add any remaining text after the last match
            processed.append(searchStart, result.cend());
            result = processed;
            
            // HTML/XML attributes - use regex_search instead of regex_replace with lambda
            std::regex attrRegex("\\s([a-zA-Z][a-zA-Z0-9_:-]*)=");
            processed.clear();
            searchStart = result.cbegin();
            std::smatch attrMatch;
            
            // Process all attribute matches
            while (std::regex_search(searchStart, result.cend(), attrMatch, attrRegex)) {
                // Add the text before the match
                processed.append(searchStart, attrMatch[0].first);
                
                // Add the space before attribute name
                processed.append(" ");
                
                // Add the highlighted attribute name
                processed.append(syntaxColors.function + attrMatch[1].str() + syntaxColors.normal);
                
                // Add the equals sign
                processed.append("=");
                
                // Move forward
                searchStart = attrMatch[0].second;
            }
            
            // Add any remaining text after the last match
            processed.append(searchStart, result.cend());
            result = processed;
            
            // HTML/XML attribute values - this one works fine with standard regex_replace
            std::regex valueRegex("=\"([^\"]*?)\"");
            result = std::regex_replace(result, 
                valueRegex, 
                "=\"" + syntaxColors.string + "$1" + syntaxColors.normal + "\"");
        }
        else if (lang == "css") {
            // CSS selectors
            std::regex selectorRegex("([\\w\\-:,.#\\[\\]=~^$*|]+)\\s*\\{");
            result = std::regex_replace(result, selectorRegex, 
                syntaxColors.keyword + "$1" + syntaxColors.normal + " {");

            // CSS properties
            std::regex propertyRegex("([\\-\\w]+)\\s*:");
            result = std::regex_replace(result, propertyRegex, 
                syntaxColors.function + "$1" + syntaxColors.normal + ":");

            // CSS values
            std::regex valueRegex(":\\s*([^;\\{\\}]+)");
            result = std::regex_replace(result, valueRegex, 
                ": " + syntaxColors.string + "$1" + syntaxColors.normal);

            // CSS important
            std::regex importantRegex("(!important)");
            result = std::regex_replace(result, importantRegex, 
                syntaxColors.keyword + "$1" + syntaxColors.normal);

            // CSS comments
            std::regex commentRegex("/\\*[^*]*\\*+([^/*][^*]*\\*+)*/");
            result = std::regex_replace(result, commentRegex, 
                syntaxColors.comment + "$&" + syntaxColors.normal);

            // CSS @ rules
            std::regex atRuleRegex("(@[\\w-]+)");
            result = std::regex_replace(result, atRuleRegex, 
                syntaxColors.keyword + "$1" + syntaxColors.normal);

            // CSS units
            std::regex unitRegex("(\\d+)(px|em|rem|vh|vw|%)");
            result = std::regex_replace(result, unitRegex, 
                syntaxColors.number + "$1" + syntaxColors.keyword + "$2" + syntaxColors.normal);

            // CSS colors
            std::regex colorRegex("#[a-fA-F0-9]{3,6}");
            result = std::regex_replace(result, colorRegex, 
                syntaxColors.number + "$&" + syntaxColors.normal);
        }
        else if (lang == "go" || lang == "golang") {
            // Go keywords
            std::vector<std::string> keywords = {"package", "import", "func", "type", "struct", "interface", 
                "map", "chan", "const", "var", "go", "defer", "select", "case", "default", "switch", 
                "range", "fallthrough", "goto"};
            keywords.insert(keywords.end(), commonKeywords.begin(), commonKeywords.end());
            
            // Go types
            std::vector<std::string> types = {"string", "int", "int8", "int16", "int32", "int64", "uint", 
                "uint8", "uint16", "uint32", "uint64", "float32", "float64", "complex64", "complex128", 
                "byte", "rune", "bool", "error"};
            
            // Go values
            std::vector<std::string> values = {"true", "false", "nil", "iota"};
            
            // Go numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Go strings
            std::regex stringRegex("([`'\"])(.*?)\\1");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // Go comments
            std::regex lineCommentRegex("//.*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // Functions
            std::regex funcRegex("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
            result = std::regex_replace(result, funcRegex, syntaxColors.function + "$1" + syntaxColors.normal + "(");
            
            // Apply keywords
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
            
            // Apply types
            for (const auto& type : types) {
                std::regex typeRegex("\\b" + type + "\\b");
                result = std::regex_replace(result, typeRegex, syntaxColors.keyword + type + syntaxColors.normal);
            }
            
            // Apply values
            for (const auto& value : values) {
                std::regex valueRegex("\\b" + value + "\\b");
                result = std::regex_replace(result, valueRegex, syntaxColors.number + value + syntaxColors.normal);
            }
        }
        else if (lang == "rust" || lang == "rs") {
            // Rust keywords
            std::vector<std::string> keywords = {"as", "async", "await", "break", "const", "continue", "crate", 
                "dyn", "else", "enum", "extern", "false", "fn", "for", "if", "impl", "in", "let", "loop", 
                "match", "mod", "move", "mut", "pub", "ref", "return", "self", "Self", "static", "struct", 
                "super", "trait", "true", "type", "unsafe", "use", "where", "while"};
            
            // Rust macros
            std::vector<std::string> macros = {"println", "panic", "vec", "format"};
            
            // Rust values
            std::vector<std::string> values = {"true", "false", "None", "Some", "Ok", "Err"};
            
            // Rust numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Rust strings
            std::regex stringRegex("\".*?\"");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // Rust character literals
            std::regex charRegex("'.'");
            result = std::regex_replace(result, charRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // Rust comments
            std::regex lineCommentRegex("//.*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // Rust macros
            for (const auto& macro : macros) {
                std::regex macroRegex(macro + "!");
                result = std::regex_replace(result, macroRegex, syntaxColors.function + macro + syntaxColors.normal + "!");
            }
            
            // Rust functions
            std::regex funcRegex("\\b(fn|impl)\\s+([a-zA-Z_][a-zA-Z0-9_]*)");
            result = std::regex_replace(result, funcRegex, 
                syntaxColors.keyword + "$1" + syntaxColors.normal + " " + 
                syntaxColors.function + "$2" + syntaxColors.normal);
            
            // Apply keywords
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
            
            // Apply values
            for (const auto& value : values) {
                std::regex valueRegex("\\b" + value + "\\b");
                result = std::regex_replace(result, valueRegex, syntaxColors.number + value + syntaxColors.normal);
            }
        }
        else if (lang == "sql") {
            // SQL keywords
            std::vector<std::string> keywords = {"SELECT", "FROM", "WHERE", "INSERT", "UPDATE", "DELETE", 
                "CREATE", "ALTER", "DROP", "TABLE", "DATABASE", "VIEW", "INDEX", "INTO", "VALUES", "AND", 
                "OR", "NOT", "JOIN", "LEFT", "RIGHT", "OUTER", "INNER", "FULL", "GROUP", "BY", "HAVING", 
                "ORDER", "ASC", "DESC", "LIMIT", "OFFSET", "SET", "TRANSACTION", "COMMIT", "ROLLBACK", 
                "GRANT", "REVOKE", "ON", "TO", "WITH", "AS", "DISTINCT", "CASE", "WHEN", "THEN", "ELSE", "END"};
            
            // Make SQL case insensitive
            for (auto& kw : keywords) {
                std::string kwLower = kw;
                std::transform(kwLower.begin(), kwLower.end(), kwLower.begin(), ::tolower);
                keywords.push_back(kwLower);
            }
            
            // SQL numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // SQL strings
            std::regex stringRegex("'.*?'");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // SQL comments
            std::regex lineCommentRegex("--.*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // Apply keywords
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        else if (lang == "php") {
            // PHP keywords
            std::vector<std::string> keywords = {"abstract", "and", "array", "as", "catch", "class", 
                "clone", "const", "declare", "default", "die", "do", "echo", "else", "elseif", "empty", 
                "enddeclare", "endfor", "endforeach", "endif", "endswitch", "endwhile", "eval", "exit", 
                "extends", "final", "finally", "fn", "for", "foreach", "function", "global", "goto", "if", 
                "implements", "include", "include_once", "instanceof", "insteadof", "interface", "isset", 
                "list", "match", "namespace", "new", "or", "print", "private", "protected", "public", 
                "require", "require_once", "return", "static", "switch", "throw", "trait", "try", "unset", 
                "use", "var", "while", "xor", "yield"};
            keywords.insert(keywords.end(), commonKeywords.begin(), commonKeywords.end());
            
            // PHP numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // PHP values
            std::vector<std::string> values = {"true", "false", "null"};
            
            // PHP variables
            std::regex varRegex("\\$[a-zA-Z_][a-zA-Z0-9_]*");
            result = std::regex_replace(result, varRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // PHP strings
            std::regex stringRegex("(['\"])(.*?)\\1");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // PHP comments
            std::regex lineCommentRegex("(//|#).*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // PHP functions
            std::regex funcRegex("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
            result = std::regex_replace(result, funcRegex, syntaxColors.function + "$1" + syntaxColors.normal + "(");
            
            // Apply keywords
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
            
            // Apply values
            for (const auto& value : values) {
                std::regex valueRegex("\\b" + value + "\\b");
                result = std::regex_replace(result, valueRegex, syntaxColors.number + value + syntaxColors.normal);
            }
        }
        else if (lang == "ruby" || lang == "rb") {
            // Ruby keywords
            std::vector<std::string> keywords = {"alias", "and", "BEGIN", "begin", "break", "case", "class", 
                "def", "defined?", "do", "else", "elsif", "END", "end", "ensure", "false", "for", "if", 
                "in", "module", "next", "nil", "not", "or", "redo", "rescue", "retry", "return", "self", 
                "super", "then", "true", "undef", "unless", "until", "when", "while", "yield"};
            
            // Ruby numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Ruby special variables
            std::regex varRegex("[@$][a-zA-Z_][a-zA-Z0-9_]*");
            result = std::regex_replace(result, varRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Ruby symbols
            std::regex symbolRegex(":[a-zA-Z_][a-zA-Z0-9_]*");
            result = std::regex_replace(result, symbolRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Ruby strings
            std::regex stringRegex("(['\"])(.*?)\\1");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // Ruby comments
            std::regex commentRegex("#.*$");
            result = std::regex_replace(result, commentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // Apply keywords
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        else if (lang == "shell" || lang == "sh" || lang == "bash") {
            // Shell keywords
            std::vector<std::string> keywords = {"if", "then", "else", "elif", "fi", "case", "esac", "for", 
                "while", "until", "do", "done", "in", "function", "time", "select", "break", "continue", 
                "return", "exit", "export", "local", "readonly", "shift", "source", "alias", "unalias"};
            
            // Shell numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Shell variables
            std::regex varRegex("\\$([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+|[#@*?$!-])");
            result = std::regex_replace(result, varRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Shell strings
            std::regex stringRegex("(['\"])(.*?)\\1");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // Shell comments
            std::regex commentRegex("#.*$");
            result = std::regex_replace(result, commentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // Shell commands
            std::regex commandRegex("^\\s*([a-zA-Z_][a-zA-Z0-9_-]*)");
            result = std::regex_replace(result, commandRegex, syntaxColors.function + "$1" + syntaxColors.normal);
            
            // Apply keywords
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        // Added new language support with number highlighting
        else if (lang == "swift") {
            // Swift numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Swift keywords and basic highlighting
            std::vector<std::string> keywords = {"func", "let", "var", "if", "else", "for", "while", "return", "class", "struct", "enum", "protocol", "import", "extension"};
            std::regex stringRegex("\"(\\\\.|[^\"])*\"");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            std::regex lineCommentRegex("//.*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            std::regex blockCommentRegex("/\\*[^*]*\\*+([^/*][^*]*\\*+)*/");
            result = std::regex_replace(result, blockCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            std::regex funcRegex("\\bfunc\\s+([a-zA-Z_][a-zA-Z0-9_]*)");
            result = std::regex_replace(result, funcRegex, syntaxColors.keyword + "func" + syntaxColors.normal + " " + syntaxColors.function + "$1" + syntaxColors.normal);
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        else if (lang == "kotlin") {
            // Kotlin numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Kotlin keywords and basic highlighting
            std::vector<std::string> keywords = {"fun", "val", "var", "if", "else", "when", "class", "object", "interface", "for", "while", "return", "import", "package"};
            std::regex stringRegex("\"(\\\\.|[^\"])*\"");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            std::regex lineCommentRegex("//.*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            std::regex funcRegex("\\bfun\\s+([a-zA-Z_][a-zA-Z0-9_]*)");
            result = std::regex_replace(result, funcRegex, syntaxColors.keyword + "fun" + syntaxColors.normal + " " + syntaxColors.function + "$1" + syntaxColors.normal);
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        else if (lang == "haskell") {
            // Haskell numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Haskell keywords and basic highlighting
            std::vector<std::string> keywords = {"module", "import", "where", "do", "let", "in", "if", "then", "else", "case", "of"};
            std::regex stringRegex("\"(\\\\.|[^\"])*\"");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            std::regex lineCommentRegex("--.*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            std::regex funcRegex("^([a-z][a-zA-Z0-9_']*)\\s*=");
            result = std::regex_replace(result, funcRegex, syntaxColors.function + "$1" + syntaxColors.normal + " =");
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        else if (lang == "lua") {
            // Lua numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Lua keywords and basic highlighting
            std::vector<std::string> keywords = {"function", "local", "end", "if", "then", "else", "elseif", "for", "in", "do", "repeat", "until", "return"};
            std::regex stringRegex("(['\"])(.*?)\\1");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            std::regex commentRegex("--.*$");
            result = std::regex_replace(result, commentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            std::regex funcRegex("\\bfunction\\s+([a-zA-Z_][a-zA-Z0-9_]*)");
            result = std::regex_replace(result, funcRegex, syntaxColors.keyword + "function" + syntaxColors.normal + " " + syntaxColors.function + "$1" + syntaxColors.normal);
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        else if (lang == "r") {
            // R numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // R keywords and basic highlighting
            std::vector<std::string> keywords = {"if", "else", "for", "while", "repeat", "function", "in", "NULL", "TRUE", "FALSE", "NA"};
            std::regex stringRegex("(['\"])(.*?)\\1");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            std::regex commentRegex("#.*$");
            result = std::regex_replace(result, commentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            std::regex funcRegex("\\bfunction\\s*\\(");
            result = std::regex_replace(result, funcRegex, syntaxColors.keyword + "function" + syntaxColors.normal + "(");
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        else if (lang == "scala") {
            // Scala numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Scala keywords and basic highlighting
            std::vector<std::string> keywords = {"def", "val", "var", "if", "else", "match", "case", "for", "yield", "class", "object", "trait", "extends", "with", "import", "package"};
            std::regex stringRegex("\"(\\\\.|[^\"])*\"");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            std::regex lineCommentRegex("//.*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            std::regex funcRegex("\\bdef\\s+([a-zA-Z_][a-zA-Z0-9_]*)");
            result = std::regex_replace(result, funcRegex, syntaxColors.keyword + "def" + syntaxColors.normal + " " + syntaxColors.function + "$1" + syntaxColors.normal);
            for (const auto& keyword : keywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        else {
            // Generic syntax highlighting for unknown languages
            
            // Generic numbers
            result = std::regex_replace(result, numberRegex, syntaxColors.number + "$&" + syntaxColors.normal);
            
            // Generic strings
            std::regex stringRegex("(['\"])(.*?)\\1");
            result = std::regex_replace(result, stringRegex, syntaxColors.string + "$&" + syntaxColors.normal);
            
            // Generic comments (try common patterns)
            std::regex lineCommentRegex("(//|#).*$");
            result = std::regex_replace(result, lineCommentRegex, syntaxColors.comment + "$&" + syntaxColors.normal);
            
            // Functions (generic pattern)
            std::regex funcRegex("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
            result = std::regex_replace(result, funcRegex, syntaxColors.function + "$1" + syntaxColors.normal + "(");
            
            // Apply common keywords
            for (const auto& keyword : commonKeywords) {
                std::regex keywordRegex("\\b" + keyword + "\\b");
                result = std::regex_replace(result, keywordRegex, syntaxColors.keyword + keyword + syntaxColors.normal);
            }
        }
        
        return result;
    }
    
public:
    CustomCoutBuffer(std::streambuf* orig)
      : original(orig), enabled(true) {}
      
    void setEnabled(bool state) {
        enabled = state;
        if (!enabled) {
            // Write reset code to ensure no colors persist
            std::string resetCode = "\033[0m";
            for (size_t i = 0; i < resetCode.length(); ++i) {
                original->sputc(resetCode[i]);
            }
        }
        
        // Flush any content in the buffer
        sync();
    }

    std::streambuf* getOriginal() {
        return original;
    }
};

class SyntaxHighlighter : public PluginInterface {
    std::streambuf* originalCoutBuffer;
    CustomCoutBuffer* customBuffer;

public:
    SyntaxHighlighter() 
      : originalCoutBuffer(nullptr), customBuffer(nullptr) {}
      
    ~SyntaxHighlighter() {}

    std::string getName() const { return "synhigh"; }
    std::string getVersion() const { return "1.0"; }
    std::string getDescription() const { return "Applies syntax highlighting to code blocks in std::cout output"; }
    std::string getAuthor() const { return "Caden Finley";}

    bool initialize() {
        // Override std::cout only
        originalCoutBuffer = std::cout.rdbuf();
        customBuffer = new CustomCoutBuffer(originalCoutBuffer);
        customBuffer->setEnabled(true);
        std::cout.rdbuf(customBuffer);  // Removed static_cast as it's already derived from streambuf
        return true;
    }

    void shutdown() {
        // First disable highlighting before restoration
        if (customBuffer) {
            customBuffer->setEnabled(false);
        }
        
        // Restore original stream buffer
        if (originalCoutBuffer) {
            std::cout.rdbuf(originalCoutBuffer);
            originalCoutBuffer = nullptr;
        }
        
        // Delete custom buffer after restoring original stream
        if (customBuffer) {
            delete customBuffer;
            customBuffer = nullptr;
        }
    }

    bool handleCommand(std::queue<std::string>& args) {
        if (args.empty()) return false;
        
        std::string cmd = args.front();
        args.pop();
        
        if (cmd == "coutoverride" && !args.empty()) {
            std::string subCmd = args.front();
            args.pop();
            
            if (subCmd == "highlight" && !args.empty()) {
                std::string state = args.front();
                args.pop();
                
                // Enable/disable code highlighting feature for std::cout only
                bool enabled = (state == "on" || state == "true" || state == "1");
                customBuffer->setEnabled(enabled);
                
                std::cout << "Code highlighting " << (enabled ? "enabled" : "disabled") << std::endl;
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> getCommands() const {
        std::vector<std::string> cmds;
        cmds.push_back("coutoverride");
        return cmds;
    }

    std::map<std::string, std::string> getDefaultSettings() const {
        std::map<std::string, std::string> settings;
        settings["highlighting_enabled"] = "true";
        return settings;
    }
    
    void updateSetting(const std::string& key, const std::string& value) {
        if(key == "highlighting_enabled") {
            bool enabled = (value == "true" || value == "1" || value == "on");
            if (customBuffer) customBuffer->setEnabled(enabled);
        }
    }
};

// Add this to ensure the destructor is called even if the plugin system fails
IMPLEMENT_PLUGIN(SyntaxHighlighter)