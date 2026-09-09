/*
  history_concurrency_driver.c

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isocline.h"

int main(int argc, char** argv) {
    if (argc != 6) {
        return 2;
    }
    ic_set_history(argv[1], strtol(argv[2], NULL, 10));
    for (int i = 0; i < atoi(argv[4]); ++i) {
        char command[128];
        snprintf(command, sizeof(command), "%s-%d", argv[3], i);
        const ic_history_metadata_t metadata[] = {{"frequency", "0"}, {"worker", argv[3]}};
        ic_history_add_with_metadata(strcmp(argv[5], "shared") == 0 ? "shared" : command, metadata,
                                     2);
    }
    return 0;
}
