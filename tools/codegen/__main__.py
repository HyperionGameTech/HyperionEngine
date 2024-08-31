import sys
import argparse

import Codegen

def parse_arguments():
    parser = argparse.ArgumentParser(description='Hyperion Engine Codegen')
    parser.add_argument('-o', '--output', type=str, help='Output directory for generated code')
    return parser.parse_args()

def main():
    args = parse_arguments()
    state = Codegen.CodegenState('src', args.output)

    gen = Codegen.Codegen(state)
    gen.run()

    if state.is_success:
        sys.stdout.write("Codegen completed successfully.\n")
        return 0
    
    sys.stderr.write("Codegen failed with errors:\n\n")

    for error in state.errors:
        sys.stderr.write(error + '\n')

    return 1

if __name__ == '__main__':
    main()