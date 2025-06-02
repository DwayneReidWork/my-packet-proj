#from .client import CmdController
from client import CmdController
import sys

def main():
    # Initialize the CmdController (which starts the cmd2 interface)
    controller = CmdController()

    # Run the command loop (this starts the interactive CLI)
    try:
        controller.cmdloop()
    except KeyboardInterrupt:
        print("\nExiting program...")
        sys.exit(0) # Handles keyboard interrupt

if __name__ == "__main__":
    main()