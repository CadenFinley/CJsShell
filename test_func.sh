#!/Users/cadenfinley/Documents/GitHub/CJsShell/build/cjsh

inner() {
    echo "inner"
}

outer() {
    inner
    echo "outer"
}

outer