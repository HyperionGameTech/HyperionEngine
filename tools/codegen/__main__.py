import sys
import argparse
import subprocess

def ensure_module_installed(import_name, install_name=None):
    if install_name is None:
        install_name = import_name

    sys.stdout.write(f"Checking if module '{import_name}' is installed...\n")

    try:
        __import__(import_name)
    except ImportError:
        sys.stdout.write(f"Module '{import_name}' not found. Installing...\n")
        subprocess.check_call([sys.executable, "-m", "ensurepip"])
        subprocess.check_call([sys.executable, "-m", "pip", "install", install_name])
    else:
        sys.stdout.write(f"Module '{import_name}' is already installed.\n")

def parse_arguments():
    parser = argparse.ArgumentParser(description='Hyperion Engine Codegen')
    parser.add_argument('-o', '--output', type=str, help='Output directory for generated code')
    return parser.parse_args()

def main():
    import Codegen

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
    ensure_module_installed("mako.template", "Mako")

    __import__("mako.template")

    main()