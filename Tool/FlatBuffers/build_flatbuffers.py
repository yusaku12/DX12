import os
import subprocess
import sys

# スクリプトの場所
ROOT = os.path.dirname(os.path.abspath(__file__))

# パス設定
SCHEMA_DIR = os.path.abspath(os.path.join(ROOT, "../FlatBuffers/Schema"))
OUTPUT_DIR = os.path.abspath(os.path.join(ROOT, "../../Source/Generated"))
FLATC = os.path.abspath(os.path.join(ROOT, "../FlatBuffers/flatc.exe"))


def compile_schema(schema_file):
    cmd = [
        FLATC,
        "--cpp",
        "-o",
        OUTPUT_DIR,
        schema_file
    ]

    print("--------------------------------------------------")
    print("Compiling:", schema_file)
    print("Command:", " ".join(cmd))

    result = subprocess.run(cmd)

    if result.returncode != 0:
        print("FlatBuffers compile failed")
        sys.exit(1)


def main():

    print("===================================")
    print("FlatBuffers Build Tool")
    print("===================================")

    if not os.path.exists(FLATC):
        print("ERROR: flatc not found:", FLATC)
        sys.exit(1)

    if not os.path.exists(SCHEMA_DIR):
        print("ERROR: schema folder not found")
        sys.exit(1)

    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)

    compiled_count = 0

    for file in os.listdir(SCHEMA_DIR):
        if file.endswith(".fbs"):
            schema_path = os.path.join(SCHEMA_DIR, file)
            compile_schema(schema_path)
            compiled_count += 1

    if compiled_count == 0:
        print("No .fbs files found")

    print("===================================")
    print("Finished. Compiled:", compiled_count)
    print("Output:", OUTPUT_DIR)
    print("===================================")


if __name__ == "__main__":
    main()