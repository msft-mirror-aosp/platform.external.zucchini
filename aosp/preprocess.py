#!/usr/bin/env python3

# ChromeOs version of zucchini expects to find its headers
# in a specifc path. In android the include paths are slightly
# different. Since we can't change upstream source code, we
# use this script to preprocess all files(.cc and ,h) and
# re-write all include paths


def main(argv):
  if len(argv) != 3:
    print("Usage:", argv[0], "<input path> <output path>")
    return 1
  input_path = argv[1]
  output_path = argv[2]
  with open(input_path, "r") as infp, open(output_path, "w") as outfp:
    for line in infp.readlines():
      line = line.replace('#include "components/zucchini/',
                          '#include "')
      line = line.replace('#include <components/zucchini/',
                          '#include <')
      line = line.replace('#include "third_party/abseil-cpp/',
                          '#include "')
      line = line.replace('#include <third_party/abseil-cpp/',
                          '#include <')
      outfp.write(line)


if __name__ == '__main__':
  import sys
  sys.exit(main(sys.argv))
