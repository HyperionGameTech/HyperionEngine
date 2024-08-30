import sys

import Codegen

def main():
    state = Codegen.CodegenState('src', 'src/gen')

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