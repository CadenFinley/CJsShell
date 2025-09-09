#!/bin/bash
# Architecture detection script for CJsShell
# This script reliably detects the target architecture across different platforms

# Function to detect architecture
detect_architecture() {
    local force_32bit="$1"
    
    # If force 32-bit is enabled, return x86
    if [ "$force_32bit" = "true" ]; then
        echo "x86"
        return 0
    fi
    
    # Detect platform first
    local platform=$(uname -s)
    local machine=$(uname -m)
    
    case "$platform" in
        "Darwin")
            # macOS detection - need to handle Rosetta translation
            # Try to get the native architecture first
            if [ -x "$(command -v arch)" ]; then
                # Use arch command to get native architecture
                local native_arch=$(arch -arm64 uname -m 2>/dev/null || arch -x86_64 uname -m 2>/dev/null || uname -m)
                case "$native_arch" in
                    "arm64")
                        echo "arm64"
                        return 0
                        ;;
                esac
            fi
            
            # Try sysctl approach for more reliable detection
            if [ -x "$(command -v sysctl)" ]; then
                local cpu_brand=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "")
                case "$cpu_brand" in
                    *"Apple"*|*"M1"*|*"M2"*|*"M3"*)
                        echo "arm64"
                        return 0
                        ;;
                esac
            fi
            
            # Check if we can detect Apple Silicon through hardware model
            if [ -x "$(command -v system_profiler)" ]; then
                local model=$(system_profiler SPHardwareDataType 2>/dev/null | grep "Model Name" | head -1)
                case "$model" in
                    *"MacBook Air (M"*|*"MacBook Pro (M"*|*"iMac (M"*|*"Mac mini (M"*|*"Mac Studio"*|*"Mac Pro (M"*)
                        echo "arm64"
                        return 0
                        ;;
                esac
            fi
            
            # Fallback to uname result
            case "$machine" in
                "arm64")
                    echo "arm64"
                    ;;
                "x86_64")
                    echo "x86_64"
                    ;;
                *)
                    echo "x86_64"
                    ;;
            esac
            ;;
        "Linux")
            # Linux detection
            case "$machine" in
                "aarch64"|"arm64")
                    echo "arm64"
                    ;;
                "x86_64"|"amd64")
                    # Check if we're running in 32-bit mode on 64-bit system
                    if [ "$(getconf LONG_BIT)" = "32" ]; then
                        echo "x86"
                    else
                        echo "x86_64"
                    fi
                    ;;
                "i386"|"i686"|"x86")
                    echo "x86"
                    ;;
                "armv7l"|"armv6l")
                    echo "arm"
                    ;;
                *)
                    # Try to use file command on /bin/ls as fallback
                    if [ -x "$(command -v file)" ] && [ -f "/bin/ls" ]; then
                        local file_output=$(file /bin/ls)
                        case "$file_output" in
                            *"x86-64"*|*"x86_64"*)
                                echo "x86_64"
                                ;;
                            *"80386"*|*"i386"*)
                                echo "x86"
                                ;;
                            *"ARM"*|*"aarch64"*)
                                echo "arm64"
                                ;;
                            *)
                                echo "x86_64"  # Default fallback
                                ;;
                        esac
                    else
                        echo "x86_64"  # Default fallback
                    fi
                    ;;
            esac
            ;;
        "FreeBSD"|"OpenBSD"|"NetBSD")
            # BSD systems
            case "$machine" in
                "amd64"|"x86_64")
                    echo "x86_64"
                    ;;
                "i386"|"i686")
                    echo "x86"
                    ;;
                "arm64"|"aarch64")
                    echo "arm64"
                    ;;
                *)
                    echo "x86_64"  # Default fallback
                    ;;
            esac
            ;;
        "CYGWIN"*|"MINGW"*|"MSYS"*)
            # Windows environments
            case "$machine" in
                "x86_64")
                    echo "x86_64"
                    ;;
                "i686"|"i386")
                    echo "x86"
                    ;;
                *)
                    # Check PROCESSOR_ARCHITECTURE environment variable
                    case "${PROCESSOR_ARCHITECTURE:-}" in
                        "AMD64"|"x86_64")
                            echo "x86_64"
                            ;;
                        "x86"|"i386")
                            echo "x86"
                            ;;
                        "ARM64")
                            echo "arm64"
                            ;;
                        *)
                            echo "x86_64"  # Default fallback
                            ;;
                    esac
                    ;;
            esac
            ;;
        *)
            # Unknown platform, try best guess
            case "$machine" in
                "x86_64"|"amd64")
                    echo "x86_64"
                    ;;
                "aarch64"|"arm64")
                    echo "arm64"
                    ;;
                "i386"|"i686"|"x86")
                    echo "x86"
                    ;;
                *)
                    echo "x86_64"  # Default fallback
                    ;;
            esac
            ;;
    esac
}

# Main execution
if [ $# -eq 0 ]; then
    detect_architecture "false"
else
    detect_architecture "$1"
fi
